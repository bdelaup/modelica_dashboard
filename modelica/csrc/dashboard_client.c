// Client du canal côté simulation (fonction C externe, D38).
// Portable : Winsock (Windows, DLL) / POSIX (harness de test local).
// POC (D28) : la résolution par nom via l'API runtime OpenModelica (D14) se branche
// ici — voir dashboard_client_input_* : la valeur est lue dans une table nom -> valeur
// alimentée par les "cmd set" reçus du pivot ; en production, l'API runtime donne
// l'adresse de la variable du modèle compilé et la valeur y est écrite directement.
//
// Logs de diagnostic (visibles dans la sortie de simulation OMEdit / le .bat) :
//   DASHBOARD_LOG=0        : silencieux
//   DASHBOARD_LOG=1        (défaut) : événements (connect, hello/helloAck, première
//                        donnée poussée par variable, cmd reçue, erreurs)
//   DASHBOARD_LOG=2        : + un log par appel input/output (verbose, par pas)
//   DASHBOARD_LOG_FILE=chemin : journal dupliqué dans ce fichier (append)
#include "dashboard_client.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET sock_t;
#define SOCK_INV INVALID_SOCKET
static int sock_close(sock_t s) { return closesocket(s); }
static void wsa_init(void) { static int done = 0; if (!done) { WSADATA d; int r = WSAStartup(MAKEWORD(2,2), &d); if (r != 0) fprintf(stderr, "WSAStartup err=%d\n", r); done = 1; } }
static double now_ms(void) { return (double)GetTickCount64(); }
static void sleep_ms(unsigned ms) { Sleep(ms); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
typedef int sock_t;
#define SOCK_INV (-1)
static int sock_close(sock_t s) { return close(s); }
static void wsa_init(void) {}
static double now_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6; }
static void sleep_ms(unsigned ms) { struct timespec ts; ts.tv_sec = ms / 1000; ts.tv_nsec = (long)(ms % 1000) * 1000000L; nanosleep(&ts, NULL); }
#endif

#define DASH_PORT 51234
#define DASH_HOST "127.0.0.1"
#define DASH_MAX_FRAME (1u << 20) /* 1 MiB, D35 */

static sock_t g_sock = SOCK_INV;
static int g_connected = 0;
static unsigned long g_seq = 0;

/* --- Journal de diagnostic --- */
static int g_log_level = -1;      /* -1 = pas encore lu dans l'environnement */
static FILE* g_log_file = NULL;
static int g_log_done = 0;
static double g_log_t0 = 0.0;

static void log_init(void) {
    if (g_log_done) return;
    g_log_done = 1;
    g_log_t0 = now_ms();
    const char* v = getenv("DASHBOARD_LOG");
    g_log_level = (v && *v) ? atoi(v) : 1;
    const char* f = getenv("DASHBOARD_LOG_FILE");
    if (f && *f) {
        g_log_file = fopen(f, "a");
        if (!g_log_file) fprintf(stderr, "[dashboard] DASHBOARD_LOG_FILE \"%s\" non ouvrable\n", f);
    }
    atexit(dashboard_client_disconnect);
}

/* level 0 = toujours affiché ; 1 = événements ; 2 = verbose par pas. */
static void dash_log(int level, const char* fmt, ...) {
    if (g_log_done == 0) log_init();
    if (level > g_log_level) return;
    char head[64];
    snprintf(head, sizeof(head), "[dashboard %8.3fs] ", (now_ms() - g_log_t0) / 1000.0);
    va_list ap;
    va_start(ap, fmt);
    fputs(head, stderr);
    vfprintf(stderr, fmt, ap);
    fflush(stderr);
    if (g_log_file) {
        va_end(ap);
        va_start(ap, fmt);
        fputs(head, g_log_file);
        vfprintf(g_log_file, fmt, ap);
        fflush(g_log_file);
    }
    va_end(ap);
}

/* table nom -> dernière valeur reçue (POC ; en prod : adresse via API runtime, D14) */
typedef struct { char name[256]; double real_v; long long int_v; int has_value; int is_bool; int logged_push; int logged_dflt; } entry_t;
static entry_t g_entries[1024];
static int g_entry_count = 0;

static entry_t* find_entry(const char* name) {
    for (int i = 0; i < g_entry_count; ++i)
        if (strcmp(g_entries[i].name, name) == 0) return &g_entries[i];
    if (g_entry_count >= (int)(sizeof(g_entries)/sizeof(g_entries[0]))) return NULL;
    entry_t* e = &g_entries[g_entry_count++];
    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, sizeof(e->name) - 1);
    return e;
}

static int send_all(const char* data, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(g_sock, data + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    return 0;
}

static int send_line(const char* json) {
    /* D35 : trame max 1 MiB — allouée sur le tas, pas la pile :
       1 MiB de stack fait overflow sur un thread Windows (stack défaut 1 MiB). */
    char* buf = (char*)malloc(DASH_MAX_FRAME);
    if (!buf) return -1;
    int n = snprintf(buf, DASH_MAX_FRAME, "%s\n", json);
    int rc = -1;
    if (n > 0 && (size_t)n < DASH_MAX_FRAME) rc = send_all(buf, n);
    free(buf);
    return rc;
}

/*Petit échappement JSON pour les noms (pas de guillemets/backslash attendus, D23 : taille max seule en V1)*/
static void json_escape(const char* in, char* out, size_t outsz) {
    size_t o = 0;
    for (const char* p = in; *p && o + 6 < outsz; ++p) {
        if (*p == '"' || *p == '\\') { out[o++] = '\\'; out[o++] = *p; }
        else if ((unsigned char)*p < 0x20) { o += (size_t)snprintf(out + o, outsz - o, "\\u%04x", *p); }
        else out[o++] = *p;
    }
    out[o] = 0;
}

DASHBOARD_API int dashboard_client_connect(const char* variablesCsv, const char* dashboardFile) {
    dash_log(1, "connect: variablesCsv=\"%s\" dashboardFile=\"%s\"\n",
         variablesCsv ? variablesCsv : "", dashboardFile ? dashboardFile : "");
    wsa_init();
    if (g_connected) { dash_log(1, "connect: déjà connecté au dashboard manager\n"); return 0; }

    /* Le dashboard manager doit être lancé AVANT la simulation : quelques tentatives pour
       tolérer un démarrage simultané (2 s max), puis dégradation (EF-B4/D15). */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DASH_PORT);
    inet_pton(AF_INET, DASH_HOST, &addr.sin_addr);
    int attempts = 0;
    for (;;) {
        g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (g_sock == SOCK_INV) { dash_log(0, "connect: socket() a échoué\n"); return -1; }
        if (connect(g_sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) break;
#if defined(_WIN32)
        int wsa_err = WSAGetLastError();
#else
        int wsa_err = errno;
#endif
        sock_close(g_sock); g_sock = SOCK_INV;
        if (++attempts >= 10) {
            dash_log(0, "connect: échec vers %s:%d (code %d) — dashboard manager absent ?\n"
                    "  Lancer le dashboard manager AVANT la simulation ; les blocs Input\n"
                    "  utiliseront leur defaultValue et les Output seront perdus.\n",
                 DASH_HOST, DASH_PORT, wsa_err);
            return -2;
        }
        dash_log(1, "connect: dashboard manager pas encore joignable (tentative %d/10, code %d), nouvel essai dans 200 ms\n",
             attempts, wsa_err);
        sleep_ms(200);
    }
    int one = 1;
    setsockopt(g_sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));

    /* hello avec la liste des variables (échappée) */
    char esc[8192];
    char vars[8192] = "";
    size_t o = 0;
    int var_count = 0;
    vars[o++] = '[';
    if (variablesCsv && *variablesCsv) {
        char tmp[8192];
        strncpy(tmp, variablesCsv, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;
        for (char* tok = strtok(tmp, ","); tok; tok = strtok(NULL, ",")) {
            if (*tok == ' ') ++tok; /* tolère "a.b, c.d" */
            json_escape(tok, esc, sizeof(esc));
            o += (size_t)snprintf(vars + o, sizeof(vars) - o, "%s\"%s\"", o > 1 ? "," : "", esc);
            ++var_count;
        }
    }
    vars[o++] = ']';
    vars[o] = 0;
    char hello[16384];
    if (dashboardFile && *dashboardFile) {
        json_escape(dashboardFile, esc, sizeof(esc));
        snprintf(hello, sizeof(hello),
            "{\"type\":\"hello\",\"version\":0,\"role\":\"simulation\",\"variables\":%s,\"dashboardFile\":\"%s\"}",
            vars, esc);
    } else {
        snprintf(hello, sizeof(hello),
            "{\"type\":\"hello\",\"version\":0,\"role\":\"simulation\",\"variables\":%s}", vars);
    }
    if (send_line(hello) != 0) { sock_close(g_sock); g_sock = SOCK_INV; return -3; }
    dash_log(1, "hello envoyé (%d variable(s))\n", var_count);

    /* attend helloAck (ou error) avec timeout 5 s */
#if defined(_WIN32)
    DWORD tv = 5000;
    setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
    struct timeval tv; tv.tv_sec = 5; tv.tv_usec = 0;
    setsockopt(g_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif
    char buf[65536];
    int n = recv(g_sock, buf, sizeof(buf) - 1, 0);
    if (n <= 0) { dash_log(0, "helloAck: aucune réponse (recv n=%d)\n", n); sock_close(g_sock); g_sock = SOCK_INV; g_connected = 0; return -4; }
    buf[n] = 0;
    if (strstr(buf, "\"helloAck\"") == NULL) { dash_log(0, "helloAck absent, réponse du manager : %s\n", buf); sock_close(g_sock); g_sock = SOCK_INV; g_connected = 0; return -5; }
    g_connected = 1;
    dash_log(1, "helloAck reçu — connecté au dashboard manager %s:%d\n", DASH_HOST, DASH_PORT);
    return 0;
}

static void push_output(const char* name, const char* valueJson) {
    if (!g_connected || g_sock == SOCK_INV) {
        dash_log(2, "output %s=%s ignoré (non connecté)\n", name, valueJson);
        return;
    }
    char line[4096];
    snprintf(line, sizeof(line),
        "{\"type\":\"data\",\"version\":0,\"seq\":%lu,\"time\":0,\"values\":{\"%s\":%s}}",
        ++g_seq, name, valueJson);
    send_line(line);
    /* time renseigné par le harness en prod ; le pivot calcule le ratio de toute façon */
    entry_t* e = find_entry(name);
    if (e && !e->logged_push) {
        dash_log(1, "première donnée poussée : %s = %s (seq %lu)\n", name, valueJson, g_seq);
        e->logged_push = 1;
    } else {
        dash_log(2, "output %s = %s (seq %lu)\n", name, valueJson, g_seq);
    }
}

DASHBOARD_API void dashboard_client_output_real(const char* name, double value, const char* dashboardFile) {
    (void)dashboardFile;
    char v[64];
    snprintf(v, sizeof(v), "%.17g", value);
    push_output(name, v);
}

DASHBOARD_API void dashboard_client_output_integer(const char* name, int value, const char* dashboardFile) {
    (void)dashboardFile;
    char v[32];
    snprintf(v, sizeof(v), "%d", value);
    push_output(name, v);
}

DASHBOARD_API void dashboard_client_output_boolean(const char* name, int value, const char* dashboardFile) {
    (void)dashboardFile;
    push_output(name, value ? "true" : "false");
}

/* Réception d'un "cmd set" du pivot : mémorise la valeur (POC ; prod : écriture via API runtime, D14) */
static void apply_set(const char* name, const char* valueJson) {
    entry_t* e = find_entry(name);
    if (!e) return;
    if (strcmp(valueJson, "true") == 0 || strcmp(valueJson, "false") == 0) {
        e->is_bool = 1;
        e->int_v = strcmp(valueJson, "true") == 0 ? 1 : 0;
    } else if (strchr(valueJson, '.') || strchr(valueJson, 'e') || strchr(valueJson, 'E')) {
        e->real_v = strtod(valueJson, NULL);
    } else {
        e->int_v = strtoll(valueJson, NULL, 10);
    }
    e->has_value = 1;
    dash_log(1, "cmd set reçu : %s = %s\n", name, valueJson);
}

/* Lit les commandes en attente (non bloquant) : cmd set -> apply_set */
static void poll_commands(void) {
    if (g_sock == SOCK_INV || !g_connected) return;
#if defined(_WIN32)
    u_long avail = 0;
    if (ioctlsocket(g_sock, FIONREAD, &avail) != 0 || avail == 0) return;
#else
    int avail = 0;
    if (ioctl(g_sock, FIONREAD, &avail) != 0 || avail <= 0) return;
#endif
    char buf[65536];
    int n = recv(g_sock, buf, (int)sizeof(buf) - 1, MSG_PEEK);
    if (n <= 0) return;
    buf[n] = 0;
    /* ordre des clés quelconque (nlohmann trie alphabétiquement) : on cherche chaque champ
       depuis le début de la ligne, indépendamment */
    char* p = buf;
    while ((p = strstr(p, "\"cmd\":\"set\"")) != NULL) {
        char* v = strstr(p, "\"variable\":\"");
        char* val = strstr(p, "\"value\":");
        if (!v || !val) break;
        v += 12;
        char name[256];
        int i = 0;
        while (*v && *v != '"' && i < 255) name[i++] = *v++;
        name[i] = 0;
        val += 8;
        char value[128];
        int j = 0;
        while (*val && *val != ',' && *val != '}' && j < 127) value[j++] = *val++;
        value[j] = 0;
        apply_set(name, value);
        p = val;
    }
    /* consomme ce qu'on a peek */
    recv(g_sock, buf, n, 0);
}

static double input_real(const char* name, double dflt) {
    poll_commands();
    entry_t* e = find_entry(name);
    if (e && e->has_value) {
        dash_log(2, "input %s = %.17g (reçu du dashboard)\n", name, e->is_bool ? (double)e->int_v : e->real_v);
        return e->is_bool ? (double)e->int_v : e->real_v;
    }
    if (e && !e->logged_dflt) {
        dash_log(1, "input %s : aucune valeur reçue du dashboard, defaultValue=%.17g utilisé\n", name, dflt);
        e->logged_dflt = 1;
    }
    dash_log(2, "input %s = %.17g (default)\n", name, dflt);
    return dflt; /* EF-B4/D15 : jamais reçu -> defaultValue */
}

DASHBOARD_API double dashboard_client_input_real(const char* name, double defaultValue, const char* dashboardFile) {
    (void)dashboardFile;
    return input_real(name, defaultValue);
}

DASHBOARD_API int dashboard_client_input_integer(const char* name, int defaultValue, const char* dashboardFile) {
    (void)dashboardFile;
    poll_commands();
    entry_t* e = find_entry(name);
    if (e && e->has_value) {
        dash_log(2, "input %s = %d (reçu du dashboard)\n", name, (int)(e->is_bool ? e->int_v : e->real_v != 0 ? (long long)e->real_v : e->int_v));
        return (int)(e->is_bool ? e->int_v : e->real_v != 0 ? (long long)e->real_v : e->int_v);
    }
    if (e && !e->logged_dflt) {
        dash_log(1, "input %s : aucune valeur reçue du dashboard, defaultValue=%d utilisé\n", name, defaultValue);
        e->logged_dflt = 1;
    }
    return defaultValue;
}

DASHBOARD_API int dashboard_client_input_boolean(const char* name, int defaultValue, const char* dashboardFile) {
    (void)dashboardFile;
    poll_commands();
    entry_t* e = find_entry(name);
    if (e && e->has_value) {
        dash_log(2, "input %s = %d (reçu du dashboard)\n", name, e->is_bool ? (int)e->int_v : (e->real_v != 0));
        return e->is_bool ? (int)e->int_v : (e->real_v != 0);
    }
    if (e && !e->logged_dflt) {
        dash_log(1, "input %s : aucune valeur reçue du dashboard, defaultValue=%d utilisé\n", name, defaultValue);
        e->logged_dflt = 1;
    }
    return defaultValue;
}

DASHBOARD_API void dashboard_client_state(const char* state) {
    if (!g_connected) { dash_log(2, "state \"%s\" ignoré (non connecté)\n", state); return; }
    char line[256];
    snprintf(line, sizeof(line), "{\"type\":\"state\",\"version\":0,\"state\":\"%s\"}", state);
    send_line(line);
    dash_log(1, "state -> %s\n", state);
}

DASHBOARD_API int dashboard_client_command(const char* cmdJsonLine) {
    if (!g_connected) return -1;
    return send_line(cmdJsonLine);
}

DASHBOARD_API void dashboard_client_disconnect(void) {
    if (g_sock != SOCK_INV) {
        dash_log(1, "déconnexion (fin de simulation)\n");
        sock_close(g_sock); g_sock = SOCK_INV;
    }
    g_connected = 0;
    g_entry_count = 0;
}
