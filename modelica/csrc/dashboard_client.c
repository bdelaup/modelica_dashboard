// Client du canal côté simulation (fonction C externe, D38).
// Portable : Winsock (Windows, DLL) / POSIX (harness de test local).
// POC (D28) : la résolution par nom via l'API runtime OpenModelica (D14) se branche
// ici — voir dashboard_client_input_* : la valeur est lue dans une table nom -> valeur
// alimentée par les "cmd set" reçus du pivot ; en production, l'API runtime donne
// l'adresse de la variable du modèle compilé et la valeur y est écrite directement.
#include "dashboard_client.h"

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
static void wsa_init(void) { static int done = 0; if (!done) { WSADATA d; WSAStartup(MAKEWORD(2,2), &d); done = 1; } }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define SOCK_INV (-1)
static int sock_close(sock_t s) { return close(s); }
static void wsa_init(void) {}
#endif

#define DASH_PORT 51234
#define DASH_HOST "127.0.0.1"
#define DASH_MAX_FRAME (1u << 20) /* 1 MiB, D35 */

static sock_t g_sock = SOCK_INV;
static int g_connected = 0;
static unsigned long g_seq = 0;

/* table nom -> dernière valeur reçue (POC ; en prod : adresse via API runtime, D14) */
typedef struct { char name[256]; double real_v; long long int_v; int has_value; int is_bool; } entry_t;
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
    char buf[DASH_MAX_FRAME];
    int n = snprintf(buf, sizeof(buf), "%s\n", json);
    if (n <= 0 || (size_t)n >= sizeof(buf)) return -1;
    return send_all(buf, n);
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
    wsa_init();
    if (g_connected) return 0;
    g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_sock == SOCK_INV) return -1;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DASH_PORT);
    inet_pton(AF_INET, DASH_HOST, &addr.sin_addr);
    if (connect(g_sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) { sock_close(g_sock); g_sock = SOCK_INV; return -2; }
    int one = 1;
    setsockopt(g_sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));

    /* hello avec la liste des variables (échappée) */
    char esc[8192];
    char vars[8192] = "";
    size_t o = 0;
    vars[o++] = '[';
    if (variablesCsv && *variablesCsv) {
        char tmp[8192];
        strncpy(tmp, variablesCsv, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;
        for (char* tok = strtok(tmp, ","); tok; tok = strtok(NULL, ",")) {
            json_escape(tok, esc, sizeof(esc));
            o += (size_t)snprintf(vars + o, sizeof(vars) - o, "%s\"%s\"", o > 1 ? "," : "", esc);
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

    /* attend helloAck (ou error) */
    char buf[65536];
    int n = recv(g_sock, buf, sizeof(buf) - 1, 0);
    if (n <= 0) { sock_close(g_sock); g_sock = SOCK_INV; return -4; }
    buf[n] = 0;
    if (strstr(buf, "\"helloAck\"") == NULL) { sock_close(g_sock); g_sock = SOCK_INV; return -5; }
    g_connected = 1;
    return 0;
}

static void push_output(const char* name, const char* valueJson) {
    if (!g_connected) return;
    char line[4096];
    snprintf(line, sizeof(line),
        "{\"type\":\"data\",\"version\":0,\"seq\":%lu,\"time\":0,\"values\":{\"%s\":%s}}",
        ++g_seq, name, valueJson);
    /* time renseigné par le harness en prod ; le pivot calcule le ratio de toute façon */
    send_line(line);
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
}

/* Lit les commandes en attente (non bloquant) : cmd set -> apply_set */
static void poll_commands(void) {
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
    if (e && e->has_value) return e->is_bool ? (double)e->int_v : e->real_v;
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
    if (e && e->has_value) return (int)(e->is_bool ? e->int_v : e->real_v != 0 ? (long long)e->real_v : e->int_v);
    return defaultValue;
}

DASHBOARD_API int dashboard_client_input_boolean(const char* name, int defaultValue, const char* dashboardFile) {
    (void)dashboardFile;
    poll_commands();
    entry_t* e = find_entry(name);
    if (e && e->has_value) return e->is_bool ? (int)e->int_v : (e->real_v != 0);
    return defaultValue;
}

DASHBOARD_API void dashboard_client_state(const char* state) {
    if (!g_connected) return;
    char line[256];
    snprintf(line, sizeof(line), "{\"type\":\"state\",\"version\":0,\"state\":\"%s\"}", state);
    send_line(line);
}

DASHBOARD_API int dashboard_client_command(const char* cmdJsonLine) {
    if (!g_connected) return -1;
    return send_line(cmdJsonLine);
}

DASHBOARD_API void dashboard_client_disconnect(void) {
    if (g_sock != SOCK_INV) { sock_close(g_sock); g_sock = SOCK_INV; }
    g_connected = 0;
    g_entry_count = 0;
}
