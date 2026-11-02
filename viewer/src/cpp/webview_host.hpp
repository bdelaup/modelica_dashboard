// Hôte WebView2 du viewer (D9) : sert l'IHM embarquée via app://, branche le pont
// natif C++ <-> JS, embarque le pivot (viewer_core) et l'iframe éditeur draw.io (D30).
#pragma once

#include <wrl.h>
#include <WebView2.h>
#include <memory>
#include <string>
#include <windows.h>

namespace dashboard {

class WebViewHost {
public:
    WebViewHost();
    ~WebViewHost();
    // Crée la fenêtre Win32 + le WebView2, charge app://index.html.
    // Retourne false si WebView2 runtime absent.
    bool create(HINSTANCE hInstance, int nCmdShow);
    int messageLoop();
    void shutdown();

private:
    void registerAppProtocol();                    // app:// -> ressources embarquées (D9)
    void onWebMessage(const std::wstring& msg);    // JS -> C++ (cmd, loadDashboard, reload, openEditor)

    struct Impl;
    std::unique_ptr<Impl> impl_;
    HWND hwnd_ = nullptr;
};

} // namespace dashboard
