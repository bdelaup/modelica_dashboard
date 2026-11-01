// viewer.exe — point d'entrée (D16/D26). Windows V1.
// Usage : viewer.exe [chemin_dashboard.drawio] [--dev]
//   --dev : ouvre le remote-debugging-port (D9, ADR-0005) pour les devtools.
#include "webview_host.hpp"
#include <windows.h>
#include <string>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int nCmdShow) {
    dashboard::WebViewHost host;
    if (!host.create(hInstance, nCmdShow)) {
        MessageBoxW(nullptr, L"WebView2 runtime requis (présent sur Windows 10/11).", L"modelica_dashboard", MB_ICONERROR);
        return 1;
    }
    return host.messageLoop();
}
