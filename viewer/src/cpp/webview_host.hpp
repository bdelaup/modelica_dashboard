// Hôte WebView2 du viewer (D9) : sert l'IHM embarquée via app://, branche le pont
// natif C++ <-> JS, embarque le pivot (shared/) et l'iframe éditeur draw.io (D30).
#pragma once

#include <wrl.h>
#include <WebView2.h>
#include <string>

namespace dashboard {

class WebViewHost {
public:
    // Crée la fenêtre Win32 + le WebView2, charge app://index.html.
    // Retourne false si WebView2 runtime absent.
    bool create(HINSTANCE hInstance, int nCmdShow);
    int messageLoop();
    void shutdown();

private:
    void registerAppProtocol();   // app:// -> ressources embarquées (D9)
    void onWebMessage(const std::wstring& msg); // JS -> C++ (cmd, loadDashboard, reload, openEditor)
    void postToJs(const std::string& json);      // C++ -> JS (tick, simState, unconnected, dashboard)

    HWND hwnd_ = nullptr;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    std::string pendingJs_; // file d'attente avant que le JS soit prêt
};

} // namespace dashboard
