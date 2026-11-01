// Hôte WebView2 (D9/D30) : IHM embarquée via app://, pont natif PostWebMessageAsString,
// pivot embarqué (shared/), iframe éditeur draw.io embed (postMessage load/save).
#include "webview_host.hpp"
#include "dashboard/pivot.hpp"

#include <windows.h>
#include <exdisp.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>

namespace dashboard {

namespace {
WebViewHost* g_host = nullptr;

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// Sert une ressource embarquée via app:// (D9) : mapping chemin -> ressource RCDATA.
std::string resourceForPath(const std::wstring& path) {
    // path attendu : /index.html, /viewer.js, /i18n/fr.json, /drawio/... (D30/D34)
    // En production : lecture dans les ressources embarquées de l'exe (resources.rc).
    // Fallback dev : dossier src/web à côté de l'exe (flag --dev).
    static char exeDir[MAX_PATH] = {};
    if (!exeDir[0]) {
        GetModuleFileNameA(nullptr, exeDir, MAX_PATH);
        std::string s(exeDir);
        exeDir[s.find_last_of("\\/") ] = 0;
    }
    std::wstring p = path;
    while (!p.empty() && p.front() == L'/') p.erase(p.begin());
    std::string rel(p.begin(), p.end());
    std::string candidates[] = {
        std::string(exeDir) + "\\src\\web\\" + rel,
        std::string(exeDir) + "\\web\\" + rel,
    };
    for (const auto& c : candidates) {
        std::ifstream f(c, std::ios::binary);
        if (f) { std::ostringstream ss; ss << f.rdbuf(); return ss.str(); }
    }
    return {};
}
} // namespace

bool WebViewHost::create(HINSTANCE hInstance, int nCmdShow) {
    g_host = this;
    WNDCLASSA wc{};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "DashboardViewer";
    RegisterClassA(&wc);
    hwnd_ = CreateWindowExA(0, "DashboardViewer", "modelica_dashboard — viewer",
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
                            nullptr, nullptr, hInstance, nullptr);
    if (!hwnd_) return false;

    // Pivot embarqué : écoute 127.0.0.1:51234 (D2/D26)
    static Pivot pivot;
    PivotEvents ev;
    ev.onTick = [this](const json& t) { postToJs(t.dump()); };
    ev.onSimState = [this](std::string_view s) { postToJs(makeSimState(s).dump()); };
    ev.onUnconnected = [this](const std::vector<std::string>& v) { postToJs(makeUnconnected(v).dump()); };
    ev.onDashboard = [this](const json& d) { postToJs(d.dump()); };
    ev.onSimConnected = [this]() { postToJs(json{{"type","simEvent"},{"connected",true}}.dump()); };
    ev.onSimDisconnected = [this]() { postToJs(json{{"type","simEvent"},{"connected",false}}.dump()); };
    if (!pivot.start(std::move(ev))) {
        MessageBoxA(hwnd_, "Port 51234 indisponible (déjà un viewer ? D25).", "modelica_dashboard", MB_ICONERROR);
    }

    // WebView2
    if (FAILED(CreateCoreWebView2EnvironmentWithOptions(
            nullptr, nullptr, nullptr,
            Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [this](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
                    if (FAILED(hr)) return hr;
                    env->CreateCoreWebView2Controller(hwnd_,
                        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [this](HRESULT hr2, ICoreWebView2Controller* c) -> HRESULT {
                                if (FAILED(hr2)) return hr2;
                                controller_ = c;
                                controller_->get_CoreWebView2(&webview_);
                                RECT r; GetClientRect(hwnd_, &r);
                                controller_->put_Bounds(r);
                                registerAppProtocol();
                                webview_->add_WebMessageReceived(
                                    Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                        [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                            wil::unique_cotaskmem_string uri;
                                            std::wstring msg;
                                            args->TryGetWebMessageAsString(&msg);
                                            onWebMessage(msg);
                                            return S_OK;
                                        }).Get(), nullptr);
                                webview_->Navigate(L"app://index.html");
                                return S_OK;
                            }).Get());
                    return S_OK;
                }).Get()))) {
        return false;
    }
    ShowWindow(hwnd_, nCmdShow);
    return true;
}

void WebViewHost::registerAppProtocol() {
    // app:// sert les ressources embarquées ; aucune navigation externe possible (ENF-2)
    webview_->AddWebResourceRequestedFilter(L"app://*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    webview_->add_WebResourceRequested(
        Microsoft::WRL::Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                wil::com_ptr<ICoreWebView2WebResourceRequest> req;
                args->get_Request(&req);
                wil::unique_cotaskmem_string uri;
                req->get_Uri(&uri);
                std::wstring path(uri.get());
                auto content = resourceForPath(path);
                if (content.empty()) return S_OK; // 404 silencieux
                IStream* stream = SHCreateMemStream(reinterpret_cast<const BYTE*>(content.data()),
                                                    static_cast<UINT>(content.size()));
                wil::com_ptr<ICoreWebView2WebResourceResponse> resp;
                webview_->CreateWebResourceResponse(stream, 200, L"OK", L"Content-Type: text/javascript",
                                                    &resp);
                args->put_Response(resp.get());
                return S_OK;
            }).Get(), nullptr);
}

void WebViewHost::postToJs(const std::string& js) {
    if (webview_) {
        std::wstring w(js.begin(), js.end());
        webview_->PostWebMessageAsString(w.c_str());
    } else {
        pendingJs_ += js + "\n";
    }
}

void WebViewHost::onWebMessage(const std::wstring& msg) {
    // JS -> C++ : {"type":"cmd",...} §8.5, loadDashboard(path), reload(), openEditor()
    std::string s(msg.begin(), msg.end());
    auto m = json::parse(s, nullptr, false);
    if (m.is_discarded()) return;
    static Pivot* pivot = nullptr; // set au create
    if (m.value("type", "") == "cmd") {
        // relayé au pivot -> sim (le pivot est propriétaire du socket sim)
        extern Pivot& viewerPivot();
        viewerPivot().sendCommand(m);
    } else if (m.value("type", "") == "loadDashboard" && m.contains("path")) {
        viewerPivot().loadDashboardFile(m["path"].get<std::string>(), {});
    }
}

int WebViewHost::messageLoop() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return static_cast<int>(msg.wParam);
}

void WebViewHost::shutdown() {
    if (controller_) controller_->Close();
}

} // namespace dashboard
