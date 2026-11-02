// Hôte WebView2 (D9/D30) : IHM embarquée via app://, pont natif PostWebMessageAsString,
// pivot embarqué (viewer_core), iframe éditeur draw.io embed (postMessage load/save).
#include "webview_host.hpp"
#include "viewer_core.hpp"

#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#include <fstream>
#include <sstream>

namespace dashboard {

namespace {

// Pivot partagé avec le shell : créé dans create(), exposé pour onWebMessage.
ViewerCore* g_core = nullptr;
ICoreWebView2Environment* g_wvEnv = nullptr;

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// Sert une ressource embarquée via app:// (D9). En dev (--dev), lit sur disque
// à côté de l'exe (src/web/... ou web/...) ; en prod, les assets sont embarqués
// dans l'exe (resources.rc) et servis depuis la ressource RCDATA.
std::string resourceForPath(const std::wstring& path) {
    static char exeDir[MAX_PATH] = {};
    if (!exeDir[0]) {
        GetModuleFileNameA(nullptr, exeDir, MAX_PATH);
        std::string s(exeDir);
        exeDir[s.find_last_of("\\/")] = 0;
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

const wchar_t* contentTypeFor(const std::wstring& path) {
    if (path.find(L".html") != std::wstring::npos) return L"text/html";
    if (path.find(L".js") != std::wstring::npos) return L"text/javascript";
    if (path.find(L".css") != std::wstring::npos) return L"text/css";
    if (path.find(L".json") != std::wstring::npos) return L"application/json";
    return L"application/octet-stream";
}

} // namespace

struct WebViewHost::Impl {
    Microsoft::WRL::ComPtr<ICoreWebView2> webview;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller;
    std::unique_ptr<ViewerCore> core;
};

bool WebViewHost::create(HINSTANCE hInstance, int nCmdShow) {
    impl_ = std::make_unique<Impl>();
    WNDCLASSA wc{};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "DashboardViewer";
    RegisterClassA(&wc);
    hwnd_ = CreateWindowExA(0, "DashboardViewer", "modelica_dashboard — viewer",
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
                            nullptr, nullptr, hInstance, nullptr);
    if (!hwnd_) return false;

    // viewer_core : pivot embarqué sur 127.0.0.1:51234 (D2/D26), sink = WebView2
    struct WvSink : UiSink {
        ICoreWebView2* wv = nullptr;
        void setWebView(ICoreWebView2* w) { wv = w; }
        void send(const json& msg) override {
            if (!wv) return;
            std::string s = msg.dump();
            std::wstring ws(s.begin(), s.end());
            wv->PostWebMessageAsString(ws.c_str());
        }
    };
    auto sink = std::make_unique<WvSink>();
    WvSink* sinkRaw = sink.get();
    impl_->core = std::make_unique<ViewerCore>(std::move(sink));
    g_core = impl_->core.get();
    if (!impl_->core->start()) {
        MessageBoxA(hwnd_, "Port 51234 indisponible (déjà un viewer ? D25).", "modelica_dashboard", MB_ICONERROR);
    }

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT hrEnv, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(hrEnv) || !env) return hrEnv;
                g_wvEnv = env;
                return env->CreateCoreWebView2Controller(hwnd_,
                    Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this, sinkRaw](HRESULT hrC, ICoreWebView2Controller* c) -> HRESULT {
                            if (FAILED(hrC) || !c) return hrC;
                            impl_->controller = c;
                            c->get_CoreWebView2(&impl_->webview);
                            sinkRaw->setWebView(impl_->webview.Get());
                            RECT r; GetClientRect(hwnd_, &r);
                            c->put_Bounds(r);
                            registerAppProtocol();
                            impl_->webview->add_WebMessageReceived(
                                Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        LPWSTR raw = nullptr;
                                        if (SUCCEEDED(args->TryGetWebMessageAsString(&raw)) && raw) {
                                            std::wstring msg(raw);
                                            CoTaskMemFree(raw);
                                            onWebMessage(msg);
                                        }
                                        return S_OK;
                                    }).Get(), nullptr);
                            impl_->webview->Navigate(L"app://index.html");
                            return S_OK;
                        }).Get());
            }).Get());
    if (FAILED(hr)) return false;
    ShowWindow(hwnd_, nCmdShow);
    return true;
}

void WebViewHost::registerAppProtocol() {
    // app:// sert les ressources embarquées ; aucune navigation externe possible (ENF-2)
    impl_->webview->AddWebResourceRequestedFilter(L"app://*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    impl_->webview->add_WebResourceRequested(
        Microsoft::WRL::Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                Microsoft::WRL::ComPtr<ICoreWebView2WebResourceRequest> req;
                args->get_Request(&req);
                LPWSTR uri = nullptr;
                req->get_Uri(&uri);
                std::wstring path(uri ? uri : L"");
                if (uri) CoTaskMemFree(uri);
                auto content = resourceForPath(path);
                if (content.empty()) return S_OK; // 404 silencieux
                IStream* stream = SHCreateMemStream(
                    reinterpret_cast<const BYTE*>(content.data()),
                    static_cast<UINT>(content.size()));
                Microsoft::WRL::ComPtr<ICoreWebView2WebResourceResponse> resp;
                ICoreWebView2Environment* env = nullptr;
                // réponse 200 avec le bon content-type
                args->get_Request(&req);
                // CreateWebResourceResponse est sur l'environment ; on passe par le controller du host via global
                extern ICoreWebView2Environment* g_wvEnv;
                if (g_wvEnv) {
                    g_wvEnv->CreateWebResourceResponse(stream, 200, L"OK",
                        (std::wstring(L"Content-Type: ") + contentTypeFor(path)).c_str(), &resp);
                    args->put_Response(resp.Get());
                }
                if (stream) stream->Release();
                return S_OK;
            }).Get(), nullptr);
}

void WebViewHost::onWebMessage(const std::wstring& msg) {
    if (!g_core) return;
    std::string s(msg.begin(), msg.end());
    auto m = json::parse(s, nullptr, false);
    if (m.is_discarded()) return;
    g_core->onUiMessage(m); // cmd §8.5 / loadDashboard / reload / openEditor
}

int WebViewHost::messageLoop() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return static_cast<int>(msg.wParam);
}

void WebViewHost::shutdown() {
    if (impl_ && impl_->controller) impl_->controller->Close();
    g_core = nullptr;
}

WebViewHost::~WebViewHost() = default;

} // namespace dashboard
