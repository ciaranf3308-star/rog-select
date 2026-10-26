// ROG Select 1.0.4 - lightweight native Windows boot selector
// Win32 + Direct2D + WIC + DirectWrite + XInput + MCI (MP3).
// No Electron, browser, Python, web server, service, telemetry,
// library scanning, auto-update, shell replacement, or DLL injection.
// See README.md and ROG-INSTALL.md. Xbox Mode / Gaming Home
// integration is UNVERIFIED on real hardware - see README.md.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <cstdio>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <Xinput.h>
#include <mmsystem.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "Xinput.lib")
#pragma comment(lib, "winmm.lib")

#define WM_SHOW_SELECTOR    (WM_USER + 100)
#define TIMER_CONTROLLER 1
#define TIMER_TRANSITION 2
// Desktop transition: poll every 250 ms, max 10 s. Device tests showed
// ACTIVE -> INACTIVE in ~1 s and ~5 s, so 10 s is bounded but generous.
// No retry, no second SendInput on timeout (that would toggle back).

static HWND g_hwnd = nullptr;
static std::string g_exeDir;
static std::string g_configPath;
static bool g_windowed = false;
static int g_focus = 1; // 0 Windows, 1 Emulation, 2 Xbox, 3 Steam (hero default)
static std::string g_status;
static bool g_soundEnabled = true;
static bool g_exitOnLaunch = false; // Test flag only, default off (see selector.ini).
static bool g_timerActive = false;
static bool g_desktopTransition = false; // Win+F11 sent, waiting for INACTIVE.
static int g_transitionTicks = 0;
static int g_activeController = -1; // XInput slot currently driving navigation.

static ID2D1Factory* g_factory = nullptr;
static ID2D1HwndRenderTarget* g_target = nullptr;
static IWICImagingFactory* g_wic = nullptr;
static IDWriteFactory* g_write = nullptr;
static IDWriteTextFormat* g_fmtTitle = nullptr;
static IDWriteTextFormat* g_fmtSub = nullptr;
static IDWriteTextFormat* g_fmtHeader = nullptr;
static IDWriteTextFormat* g_fmtFooter = nullptr;
static IDWriteTextFormat* g_fmtError = nullptr;
static ID2D1Bitmap* g_background = nullptr;

struct Entry {
    std::string id, label, imageFile, action, path, arguments, returnProcess;
    ID2D1Bitmap* bitmap = nullptr;
    D2D1_COLOR_F accent = D2D1::ColorF(D2D1::ColorF::White);
};
static std::vector<Entry> g_entries;
static std::string g_backgroundFile = "assets\\background.png";
static std::string g_navigateFile = "assets\\navigate.mp3";
static std::string g_selectFile = "assets\\select.mp3";

// ---------- helpers ----------
static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
static std::string Lower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}
static std::wstring ToWide(const std::string& s); // Defined below, used by FileExists.
static bool FileExists(const std::string& p) {
    // Unicode (W) file API, so a non-ASCII exe/user path does not fail
    // the check while the WIC loader (also W) would have succeeded.
    DWORD a = GetFileAttributesW(ToWide(p).c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
static std::string JoinExe(const std::string& rel) {
    if (rel.size() >= 2 && rel[1] == ':') return rel;
    if (!rel.empty() && (rel[0] == '\\' || rel[0] == '/')) return rel;
    return g_exeDir + "\\" + rel;
}
static std::wstring ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}
static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    if (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}
static void Log(const std::string& msg) {
    SYSTEMTIME t; GetLocalTime(&t);
    char buf[64];
    sprintf_s(buf, "%04d-%02d-%02d %02d:%02d:%02d ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    std::string line = std::string(buf) + msg + "\r\n";
    OutputDebugStringW(ToWide(line).c_str());
    if (g_exeDir.empty()) return;
    std::wstring path = ToWide(g_exeDir + "\\rog-select.log");
    // Simple rotation: never let the log grow without bound. W APIs so
    // a non-ASCII exe path still logs instead of silently failing.
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER sz; if (GetFileSizeEx(h, &sz) && sz.QuadPart > 1024 * 1024) {
            CloseHandle(h);
            DeleteFileW(ToWide(g_exeDir + "\\rog-select.old.log").c_str());
            MoveFileW(path.c_str(), ToWide(g_exeDir + "\\rog-select.old.log").c_str());
        } else CloseHandle(h);
    }
    HANDLE out = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(out, line.c_str(), (DWORD)line.size(), &written, nullptr);
        CloseHandle(out);
    }
}
static bool ProcessExists(const std::string& exeName) {
    // Wide version only: PROCESSENTRY32A / Process32FirstA failed to compile
    // on the GitHub MSVC runner (SDK 10.0.26100, VS 18). The W API is the
    // same snapshot walk, with a wide exe name.
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
    std::wstring want = ToWide(exeName);
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do { if (_wcsicmp(pe.szExeFile, want.c_str()) == 0) { found = true; break; } }
        while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}
// Xbox Mode / Full Screen Experience detection.
// Microsoft GDK guidance: call IsGamingFullScreenExperienceActive() from
// <gamingexperience.h>, linked via windowsapp.lib, SDK 10.0.26100.3916+,
// guarded by IsApiSetImplemented("api-ms-win-gaming-experience-l1-1-0").
// This app is a classic Win32 exe, so it loads the API set dynamically:
// if the API set is absent (older Windows, non-handheld build), FSE state
// is reported as UNAVAILABLE, never guessed. FSE active does NOT prove
// that every optimisation is still in effect after a desktop visit -
// that is a device test, not a return value.
typedef BOOL (WINAPI* PFN_IsFSEActive)(void);
static bool QueryFse(bool* available) {
    *available = false;
    HMODULE h = LoadLibraryA("api-ms-win-gaming-experience-l1-1-0.dll");
    if (!h) return false;
    auto fn = (PFN_IsFSEActive)GetProcAddress(h, "IsGamingFullScreenExperienceActive");
    if (!fn) return false;
    *available = true;
    return fn() != FALSE;
}
static void LogFseState(const char* when) {
    bool avail = false;
    bool active = QueryFse(&avail);
    if (!avail) Log(std::string("FSE state (") + when + "): UNAVAILABLE - API set not present on this build");
    else Log(std::string("FSE state (") + when + "): " + (active ? "ACTIVE" : "INACTIVE"));
}
static void PlayMp3(const std::string& relFile) {
    if (!g_soundEnabled) return;
    std::string full = JoinExe(relFile);
    if (!FileExists(full)) { Log("Sound missing, skipping: " + full); return; }
    // W API: an MCI command built from an ANSI path could garble a
    // non-ASCII exe path; the wide command cannot.
    mciSendStringW(L"close rogselect_sound", nullptr, 0, nullptr);
    std::wstring open = L"open \"" + ToWide(full) + L"\" type mpegvideo alias rogselect_sound";
    if (mciSendStringW(open.c_str(), nullptr, 0, nullptr) != 0) {
        Log("Sound open failed: " + full); return;
    }
    mciSendStringW(L"play rogselect_sound", nullptr, 0, nullptr);
}

// ---------- config ----------
static void LoadConfig() {
    // Defaults first, so a missing/corrupt file still shows a usable selector.
    g_entries.clear();
    Entry w; w.id = "windows"; w.label = "Windows"; w.imageFile = "assets\\windows.png";
    w.action = "desktop"; w.accent = D2D1::ColorF(0.0f, 0.6f, 1.0f);
    Entry e; e.id = "emulation"; e.label = "Emulation"; e.imageFile = "assets\\emulation.png";
    e.action = "exe"; e.path = ""; e.accent = D2D1::ColorF(0.0f, 1.0f, 0.65f);
    Entry x; x.id = "xbox"; x.label = "Xbox"; x.imageFile = "assets\\xbox.png";
    x.action = "store_app"; x.path = "shell:AppsFolder\\Microsoft.GamingApp_8wekyb3d8bbwe!Microsoft.Xbox.App";
    x.returnProcess = "GamingApp.exe"; x.accent = D2D1::ColorF(0.1f, 0.9f, 0.3f);
    Entry s; s.id = "steam"; s.label = "Steam"; s.imageFile = "assets\\steam.png";
    s.action = "exe"; s.path = ""; s.arguments = "-bigpicture"; s.returnProcess = "steam.exe";
    s.accent = D2D1::ColorF(0.4f, 0.7f, 1.0f);
    g_entries.push_back(w); g_entries.push_back(e); g_entries.push_back(x); g_entries.push_back(s);

    std::ifstream in(ToWide(g_configPath)); // W path: non-ASCII config path still loads.
    if (!in) { Log("Config not found, using defaults: " + g_configPath); return; }
    std::map<std::string, std::map<std::string, std::string>> cfg;
    std::string section = "general", line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') { section = Lower(Trim(line.substr(1, line.size() - 2))); continue; }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        cfg[section][Lower(Trim(line.substr(0, eq)))] = Trim(line.substr(eq + 1));
    }
    auto get = [&](const std::string& sec, const std::string& key, std::string& out) {
        auto it = cfg.find(sec); if (it == cfg.end()) return;
        auto k = it->second.find(key); if (k != it->second.end() && !k->second.empty()) out = k->second;
    };
    get("general", "background", g_backgroundFile);
    get("general", "sound_navigate", g_navigateFile);
    get("general", "sound_select", g_selectFile);
    { std::string v; get("general", "default_focus", v); if (!v.empty()) { int f = atoi(v.c_str()); if (f >= 0 && f <= 3) g_focus = f; } }
    { std::string v; get("general", "sound", v); if (Lower(v) == "off" || v == "0") g_soundEnabled = false; }
    { std::string v; get("general", "exit_on_launch", v); if (Lower(v) == "on" || v == "1" || Lower(v) == "true") g_exitOnLaunch = true; }
    for (auto& en : g_entries) {
        get(en.id, "label", en.label);
        get(en.id, "image", en.imageFile);
        get(en.id, "action", en.action);
        get(en.id, "path", en.path);
        get(en.id, "arguments", en.arguments);
        get(en.id, "return_process", en.returnProcess);
    }
    Log("Config loaded: " + g_configPath);
}

// ---------- Direct2D / WIC ----------
static HRESULT LoadBitmap(const std::string& relFile, ID2D1Bitmap** out) {
    *out = nullptr;
    std::string full = JoinExe(relFile);
    if (!FileExists(full)) { Log("Asset missing: " + full); return E_FAIL; }
    IWICBitmapDecoder* dec = nullptr;
    HRESULT hr = g_wic->CreateDecoderFromFilename(ToWide(full).c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &dec);
    if (FAILED(hr)) { Log("WIC decode failed: " + full); return hr; }
    IWICBitmapFrameDecode* frame = nullptr;
    hr = dec->GetFrame(0, &frame);
    if (SUCCEEDED(hr)) {
        IWICFormatConverter* conv = nullptr;
        hr = g_wic->CreateFormatConverter(&conv);
        if (SUCCEEDED(hr)) {
            hr = conv->Initialize(frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                nullptr, 0.0, WICBitmapPaletteTypeMedianCut);
            if (SUCCEEDED(hr)) hr = g_target->CreateBitmapFromWicBitmap(conv, nullptr, out);
            conv->Release();
        }
        frame->Release();
    }
    dec->Release();
    if (FAILED(hr)) Log("Bitmap create failed: " + full);
    return hr;
}
static void CreateTextFormats() {
    if (!g_write) return;
    g_write->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 44.0f, L"en-us", &g_fmtTitle);
    g_write->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"en-us", &g_fmtSub);
    g_write->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &g_fmtHeader);
    g_write->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"en-us", &g_fmtFooter);
    g_write->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &g_fmtError);
    if (g_fmtTitle) g_fmtTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    if (g_fmtSub) g_fmtSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    if (g_fmtFooter) g_fmtFooter->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    if (g_fmtError) g_fmtError->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
}
static bool CreateDeviceResources() {
    if (g_target) return true;
    RECT rc; GetClientRect(g_hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU((UINT32)(rc.right - rc.left), (UINT32)(rc.bottom - rc.top));
    if (size.width == 0 || size.height == 0) return false;
    // v1.0.4 DPI fix: explicit 96 DPI, so 1 Direct2D unit = 1 physical
    // pixel. The default (0,0) let the render target inherit the system
    // DPI, which mismatched the physical client rect at 175% scaling
    // and made layout/hit-testing disagree with what was drawn.
    HRESULT hr = g_factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0f, 96.0f),
        D2D1::HwndRenderTargetProperties(g_hwnd, size), &g_target);
    if (FAILED(hr)) { Log("CreateHwndRenderTarget failed"); return false; }
    g_target->SetDpi(96.0f, 96.0f);
    CreateTextFormats();
    LoadBitmap(g_backgroundFile, &g_background);
    for (auto& en : g_entries) LoadBitmap(en.imageFile, &en.bitmap);
    return true;
}
static void DiscardDeviceResources() {
    if (g_background) { g_background->Release(); g_background = nullptr; }
    for (auto& en : g_entries) if (en.bitmap) { en.bitmap->Release(); en.bitmap = nullptr; }
    if (g_fmtTitle) { g_fmtTitle->Release(); g_fmtTitle = nullptr; }
    if (g_fmtSub) { g_fmtSub->Release(); g_fmtSub = nullptr; }
    if (g_fmtHeader) { g_fmtHeader->Release(); g_fmtHeader = nullptr; }
    if (g_fmtFooter) { g_fmtFooter->Release(); g_fmtFooter = nullptr; }
    if (g_fmtError) { g_fmtError->Release(); g_fmtError = nullptr; }
    if (g_target) { g_target->Release(); g_target = nullptr; }
}
static std::vector<D2D1_RECT_F> CalcCards(int cw, int ch) {
    std::vector<D2D1_RECT_F> out;
    float gap = (float)cw * 0.016f;
    float baseW = ((float)cw * 0.80f - gap * 3) / 4.0f;
    float baseH = baseW * 1448.0f / 1086.0f;
    float maxH = (float)ch * 0.52f;
    if (baseH > maxH) { baseH = maxH; baseW = baseH * 1086.0f / 1448.0f; }
    float rowW = baseW * 4 + gap * 3;
    float startX = ((float)cw - rowW) / 2.0f;
    float topY = (float)ch * 0.32f;
    for (int i = 0; i < 4; i++) {
        float x = startX + i * (baseW + gap);
        float w = baseW, h = baseH, y = topY;
        if (i == g_focus) { // Focused card grows, matching the hero (Emulation larger).
            float gw = w * 0.08f, gh = h * 0.08f;
            x -= gw / 2; y -= gh / 2; w += gw; h += gh;
        }
        out.push_back(D2D1::RectF(x, y, x + w, y + h));
    }
    return out;
}
static void DrawTextCenter(ID2D1SolidColorBrush* brush, IDWriteTextFormat* fmt,
                           const std::wstring& text, float x, float y, float w, float h) {
    if (!fmt || !g_target) return;
    g_target->DrawTextW(text.c_str(), (UINT32)text.size(), fmt, D2D1::RectF(x, y, x + w, y + h), brush);
}
static void Render() {
    if (!CreateDeviceResources()) return;
    RECT rc; GetClientRect(g_hwnd, &rc);
    int cw = rc.right - rc.left, ch = rc.bottom - rc.top;
    if (cw <= 0 || ch <= 0) return;
    g_target->BeginDraw();
    g_target->Clear(D2D1::ColorF(0.04f, 0.05f, 0.06f));
    D2D1_RECT_F full = D2D1::RectF(0, 0, (float)cw, (float)ch);
    if (g_background) g_target->DrawBitmap(g_background, full);
    ID2D1SolidColorBrush* white = nullptr; ID2D1SolidColorBrush* grey = nullptr;
    g_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &white);
    g_target->CreateSolidColorBrush(D2D1::ColorF(0.65f, 0.68f, 0.72f), &grey);
    if (white && grey) {
        // Header (left-aligned format), title and subtitle - same copy as the hero target.
        DrawTextCenter(white, g_fmtHeader, L"/  ROG SELECT", (float)cw * 0.04f, (float)ch * 0.03f, (float)cw * 0.35f, 30);
        DrawTextCenter(white, g_fmtTitle, L"Choose your destination", 0, (float)ch * 0.12f, (float)cw, 60);
        DrawTextCenter(grey, g_fmtSub, L"Boot straight into what you actually use", 0, (float)ch * 0.12f + 56, (float)cw, 30);
    }
    auto cards = CalcCards(cw, ch);
    for (int i = 0; i < 4; i++) {
        auto r = cards[i];
        if (i == g_focus) {
            ID2D1SolidColorBrush* glow = nullptr; ID2D1SolidColorBrush* border = nullptr;
            D2D1_COLOR_F a = g_entries[i].accent;
            g_target->CreateSolidColorBrush(D2D1::ColorF(a.r, a.g, a.b, 0.30f), &glow);
            g_target->CreateSolidColorBrush(D2D1::ColorF(a.r, a.g, a.b, 1.0f), &border);
            if (glow) { g_target->DrawRectangle(D2D1::RectF(r.left - 6, r.top - 6, r.right + 6, r.bottom + 6), glow, 8.0f); glow->Release(); }
            if (border) { g_target->DrawRectangle(r, border, 3.0f); border->Release(); }
        }
        if (g_entries[i].bitmap) {
            g_target->DrawBitmap(g_entries[i].bitmap, r);
        } else {
            // Fallback: never leave a blank card if an asset is missing.
            ID2D1SolidColorBrush* dark = nullptr;
            g_target->CreateSolidColorBrush(D2D1::ColorF(0.10f, 0.12f, 0.15f), &dark);
            if (dark) { g_target->FillRectangle(r, dark); dark->Release(); }
            if (white) DrawTextCenter(white, g_fmtSub, ToWide(g_entries[i].label), r.left, (r.top + r.bottom) / 2 - 12, r.right - r.left, 24);
        }
    }
    if (white && grey) {
        DrawTextCenter(white, g_fmtFooter, L"A    Launch        +    Move", 0, (float)ch * 0.88f, (float)cw, 30);
        if (!g_status.empty()) {
            ID2D1SolidColorBrush* dark = nullptr;
            g_target->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.75f), &dark);
            D2D1_RECT_F bar = D2D1::RectF((float)cw * 0.15f, (float)ch * 0.82f, (float)cw * 0.85f, (float)ch * 0.82f + 28);
            if (dark) { g_target->FillRectangle(bar, dark); dark->Release(); }
            DrawTextCenter(white, g_fmtError, ToWide(g_status), bar.left, bar.top + 4, bar.right - bar.left, 24);
        }
    }
    if (white) white->Release();
    if (grey) grey->Release();
    HRESULT hr = g_target->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) DiscardDeviceResources();
}
static int CardAtPoint(int x, int y) {
    RECT rc; GetClientRect(g_hwnd, &rc);
    auto cards = CalcCards(rc.right - rc.left, rc.bottom - rc.top);
    for (int i = 0; i < 4; i++) {
        if (x >= cards[i].left && x <= cards[i].right && y >= cards[i].top && y <= cards[i].bottom) return i;
    }
    return -1;
}
static void SetFocusIndex(int idx, bool playSound) {
    idx = ((idx % 4) + 4) % 4;
    if (idx != g_focus) {
        g_focus = idx;
        if (playSound) PlayMp3(g_navigateFile);
        InvalidateRect(g_hwnd, nullptr, FALSE);
    }
}

// ---------- launch / return ----------
// Deliberately NO background watcher thread. Earlier versions waited for a
// child process to exit and then restored the selector. That is a polling
// monitor, it can pop the selector over the launched app when a launcher
// forwards to an already-running process (Steam, Xbox), and it is not how
// FSE is meant to work. In FSE, return is via the system's own surfaces:
// Game Bar Home (Win+G / Xbox button, then Home), Task View (Win+Tab,
// long-press Xbox button, swipe up on touch), or relaunching this exe,
// whose single-instance handling shows the hidden window. The selector
// therefore hides on launch and does nothing else until the user brings
// it back through one of those routes. No thread, no snapshot polling,
// no service. selector.ini return_process is now ignored (legacy only).
// v1.0.3: controller polling stops when the selector is hidden, so a
// hidden selector costs no timer wake-ups at all. The message loop
// already blocks, so with the timer stopped the hidden process is
// idle (no CPU, no painting, no polling) until it is shown again.
static void StartControllerTimer() {
    if (g_hwnd && !g_timerActive) {
        SetTimer(g_hwnd, TIMER_CONTROLLER, 80, nullptr);
        g_timerActive = true;
    }
}
static void StopControllerTimer() {
    if (g_hwnd && g_timerActive) {
        KillTimer(g_hwnd, TIMER_CONTROLLER);
        g_timerActive = false;
    }
}
static void HideSelectorOnly() {
    StopControllerTimer();
    ShowWindow(g_hwnd, SW_HIDE);
    Log("Selector hidden after launch, controller polling stopped. Return via Game Bar Home, Task View, or relaunching rog-select.exe.");
}
// Terminate-entirely option, investigated for v1.0.3 and kept behind a
// config flag for a device test (see selector.ini). Microsoft's own
// GDK launcher guidance says: after a launcher successfully launches
// a game, consider terminating the launcher (or at least minimising
// its resources) to give the game more resources. Playnite-FSE and
// AnyFSE both exit after handing off. Terminating gives 0 CPU and
// 0 RAM for the selector, but return then depends entirely on
// Windows Gaming Home relaunching the home app (Game Bar Home) or
// on manually relaunching the exe — neither is verified on this
// device yet, so exit_on_launch stays OFF by default. If Codex
// confirms Game Bar Home reliably relaunches ROG Select after an
// exit_on_launch launch, that flag can become the default in a
// later small update. It is NOT the default in v1.0.3.
static void HideOrExitAfterLaunch() {
    if (g_exitOnLaunch) {
        Log("exit_on_launch=1: selector exiting entirely after launch (no hidden process, no polling). Return depends on Gaming Home relaunch or manual relaunch.");
        PostMessage(g_hwnd, WM_CLOSE, 0, 0);
    } else {
        HideSelectorOnly();
    }
}
static std::string FindSteam() {
    const char* candidates[] = {
        "C:\\Program Files (x86)\\Steam\\steam.exe",
        "C:\\Program Files\\Steam\\steam.exe",
    };
    for (auto c : candidates) if (FileExists(c)) return c;
    char pf86[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableA("ProgramFiles(x86)", pf86, MAX_PATH)) {
        std::string p = std::string(pf86) + "\\Steam\\steam.exe";
        if (FileExists(p)) return p;
    }
    return "";
}
// v1.0.4 Windows switch, from physical-device testing (v1.0.3 on the
// Ally X): a standalone, non-admin native program successfully used
// SendInput for Win+F11 and FSE went ACTIVE -> INACTIVE in ~1 s and
// ~5 s, with no confirmation prompt and no physical keyboard. That is
// a device-tested behaviour, not a Microsoft "exit FSE" API — there
// is still no such API alongside IsGamingFullScreenExperienceActive().
// Rules, all enforced below: check FSE first; send the shortcut
// exactly once, releasing every simulated key; poll for INACTIVE for
// at most 10 s; on success, quit like the inactive path so the
// desktop is clean; on failure or unavailable status, show a safe
// manual message and never toggle again (a second Win+F11 would
// re-enter FSE). No admin, no shell replacement, no service, no
// registry change, no explorer launch while FSE is still active.
static bool SendWinF11Once() {
    INPUT in[4] = {};
    for (int i = 0; i < 4; i++) in[i].type = INPUT_KEYBOARD;
    in[0].ki.wVk = VK_LWIN;
    in[1].ki.wVk = VK_F11;
    in[2].ki.wVk = VK_F11; in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    in[3].ki.wVk = VK_LWIN; in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    UINT sent = SendInput(4, in, sizeof(INPUT));
    if (sent != 4) {
        // Release any key that may have been pressed, then fail safe.
        INPUT rel[2] = {};
        for (int i = 0; i < 2; i++) rel[i].type = INPUT_KEYBOARD;
        rel[0].ki.wVk = VK_F11; rel[0].ki.dwFlags = KEYEVENTF_KEYUP;
        rel[1].ki.wVk = VK_LWIN; rel[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, rel, sizeof(INPUT));
        Log("SendInput Win+F11 sent only " + std::to_string(sent) + " of 4 events; keys released, no retry");
        return false;
    }
    return true;
}
static void QuitToDesktop() {
    // FSE is inactive here: quitting IS the desktop switch. Explorer
    // is the shell underneath; it is never replaced.
    if (!ProcessExists("explorer.exe")) {
        STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = {};
        std::wstring exe = L"C:\\Windows\\explorer.exe";
        std::vector<wchar_t> buf(exe.begin(), exe.end()); buf.push_back(L'\0');
        if (CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
            Sleep(400);
        } else Log("Failed to start explorer.exe for desktop switch");
    }
    PostMessage(g_hwnd, WM_CLOSE, 0, 0);
}
static void LaunchEntry(int idx) {
    if (idx < 0 || idx >= (int)g_entries.size()) return;
    // Prevent double activation while a desktop transition is underway:
    // a second launch (especially a second Windows press) could toggle
    // FSE back on or launch an app into the middle of the switch.
    if (g_desktopTransition) {
        g_status = "Desktop switch in progress. Please wait.";
        InvalidateRect(g_hwnd, nullptr, FALSE);
        return;
    }
    Entry& en = g_entries[idx];
    PlayMp3(g_selectFile);
    Log("Launch: " + en.label + " action=" + en.action + " path=" + en.path);
    g_status.clear();
    if (en.action == "desktop") {
        LogFseState("Windows button");
        bool avail = false;
        bool active = QueryFse(&avail);
        if (!avail) {
            // Status unknown: do not guess, do not SendInput blindly.
            g_status = "Xbox Mode status unavailable. Press Win+F11, or Task View > Windows Desktop > Continue.";
            Log("Windows requested, FSE status unavailable: manual instruction shown, SendInput NOT sent");
            InvalidateRect(g_hwnd, nullptr, FALSE);
            return;
        }
        if (active) {
            if (!SendWinF11Once()) {
                g_status = "Desktop switch could not be started. Press Win+F11, or Task View > Windows Desktop > Continue.";
                InvalidateRect(g_hwnd, nullptr, FALSE);
                return;
            }
            g_desktopTransition = true;
            g_transitionTicks = 0;
            g_status = "Switching to Windows desktop...";
            Log("Win+F11 sent once via SendInput, polling FSE for INACTIVE (250 ms, max 10 s)");
            SetTimer(g_hwnd, TIMER_TRANSITION, 250, nullptr);
            InvalidateRect(g_hwnd, nullptr, FALSE);
            return;
        }
        QuitToDesktop();
        return;
    }
    if (en.action == "exe") {
        std::string path = en.path;
        if (path.empty() && en.id == "steam") path = FindSteam();
        if (path.empty() || !FileExists(path)) {
            g_status = en.label + " path not set or not found. Edit selector.ini and try again.";
            Log("Launch failed, path missing: " + en.label + " path=" + path);
            InvalidateRect(g_hwnd, nullptr, FALSE);
            return;
        }
        std::string dir = path.substr(0, path.find_last_of("\\/"));
        std::string cmd = "\"" + path + "\"";
        if (!en.arguments.empty()) cmd += " " + en.arguments;
        STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = {};
        si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_SHOWNORMAL;
        std::wstring wcmd = ToWide(cmd), wdir = ToWide(dir);
        std::vector<wchar_t> cmdBuf(wcmd.begin(), wcmd.end()); cmdBuf.push_back(L'\0');
        if (!CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, 0, nullptr,
                            dir.empty() ? nullptr : wdir.c_str(), &si, &pi)) {
            g_status = "Failed to launch " + en.label + ". See rog-select.log.";
            Log("CreateProcess failed for " + path);
            InvalidateRect(g_hwnd, nullptr, FALSE);
            return;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        HideOrExitAfterLaunch();
        return;
    }
    if (en.action == "uri" || en.action == "store_app") {
        if (en.path.empty()) {
            g_status = en.label + " path not set in selector.ini.";
            InvalidateRect(g_hwnd, nullptr, FALSE);
            return;
        }
        HINSTANCE r = ShellExecuteW(nullptr, L"open", ToWide(en.path).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)r <= 32) {
            g_status = "Failed to open " + en.label + ". Check its path in selector.ini and see rog-select.log.";
            Log("ShellExecute failed for " + en.path);
            InvalidateRect(g_hwnd, nullptr, FALSE);
            return;
        }
        HideOrExitAfterLaunch();
        return;
    }
    g_status = "Unknown action for " + en.label + " in selector.ini.";
    Log("Unknown action: " + en.action);
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

// ---------- window ----------
static WORD g_prevButtons = 0;
static SHORT g_prevThumbX = 0;
static ULONGLONG g_lastThumbMove = 0;
static void PollController() {
    // v1.0.4: scan slots 0-3 instead of assuming slot 0, and support
    // reconnection. The first connected slot wins. If the active slot
    // disconnects, or a different slot becomes active, the next poll
    // only records that controller's state (no phantom A/D-pad press
    // from a newly connected pad whose buttons are already held).
    XINPUT_STATE st = {};
    int slot = -1;
    for (int i = 0; i < 4; i++) {
        XINPUT_STATE cand = {};
        if (XInputGetState(i, &cand) == ERROR_SUCCESS) { slot = i; st = cand; break; }
    }
    if (slot < 0) { g_activeController = -1; g_prevButtons = 0; g_prevThumbX = 0; return; }
    if (slot != g_activeController) {
        g_activeController = slot;
        g_prevButtons = st.Gamepad.wButtons;
        g_prevThumbX = st.Gamepad.sThumbLX;
        Log("Controller active on XInput slot " + std::to_string(slot));
        return;
    }
    WORD pressed = (WORD)(st.Gamepad.wButtons & ~g_prevButtons);
    if (pressed & XINPUT_GAMEPAD_DPAD_LEFT) SetFocusIndex(g_focus - 1, true);
    if (pressed & XINPUT_GAMEPAD_DPAD_RIGHT) SetFocusIndex(g_focus + 1, true);
    if (pressed & XINPUT_GAMEPAD_DPAD_UP) SetFocusIndex(g_focus - 1, true);
    if (pressed & XINPUT_GAMEPAD_DPAD_DOWN) SetFocusIndex(g_focus + 1, true);
    if (pressed & XINPUT_GAMEPAD_A) LaunchEntry(g_focus);
    // Left stick: edge + held repeat (400 ms initial, 300 ms repeat).
    SHORT tx = st.Gamepad.sThumbLX;
    int dir = 0;
    if (tx < -16000) dir = -1; else if (tx > 16000) dir = 1;
    ULONGLONG now = GetTickCount64();
    if (dir != 0 && (dir != (g_prevThumbX < -16000 ? -1 : (g_prevThumbX > 16000 ? 1 : 0)) || now - g_lastThumbMove >= 300)) {
        if (dir != (g_prevThumbX < -16000 ? -1 : (g_prevThumbX > 16000 ? 1 : 0)) || now - g_lastThumbMove >= 400) {
            SetFocusIndex(g_focus + dir, true);
            g_lastThumbMove = now;
        }
    }
    g_prevButtons = st.Gamepad.wButtons;
    g_prevThumbX = tx;
}
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        StartControllerTimer();
        return 0;
    case WM_SHOWWINDOW:
        // ShowWindow(SW_HIDE) / SW_SHOW both arrive here. Stop polling
        // on hide, resume on show. This is the v1.0.3 idle fix: a hidden
        // selector has no timer wake-ups at all.
        if (wParam) StartControllerTimer(); else StopControllerTimer();
        return 0;
    case WM_TIMER:
        if (wParam == TIMER_CONTROLLER) PollController();
        if (wParam == TIMER_TRANSITION) {
            // Bounded desktop-transition poll. Success: FSE INACTIVE.
            // Failure: timeout (10 s) or status unavailable. Either way
            // the timer stops and no second Win+F11 is ever sent.
            g_transitionTicks++;
            bool avail = false;
            bool active = QueryFse(&avail);
            if (!avail || g_transitionTicks > 40) {
                KillTimer(hwnd, TIMER_TRANSITION);
                g_desktopTransition = false;
                g_status = "Desktop switch did not complete. Press Win+F11, or Task View > Windows Desktop > Continue.";
                Log("Desktop transition failed (unavailable or 10 s timeout): manual instruction shown, no retry");
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (!active) {
                KillTimer(hwnd, TIMER_TRANSITION);
                g_desktopTransition = false;
                Log("Desktop transition succeeded: FSE INACTIVE after " + std::to_string(g_transitionTicks * 250) + " ms, quitting to desktop");
                QuitToDesktop();
            }
            // Still ACTIVE: keep polling, status already says switching.
        }
        return 0;
    case WM_DPICHANGED: {
        // Per-Monitor V2: Windows suggests the new window rect in
        // physical pixels. Use it exactly, resize the render target,
        // repaint. Layout is recomputed from the client rect on every
        // paint, so no card is ever drawn outside the window.
        RECT* sug = (RECT*)lParam;
        if (sug) SetWindowPos(hwnd, nullptr, sug->left, sug->top, sug->right - sug->left, sug->bottom - sug->top, SWP_NOZORDER | SWP_NOACTIVATE);
        if (g_target) {
            RECT rc; GetClientRect(hwnd, &rc);
            g_target->Resize(D2D1::SizeU((UINT32)(rc.right - rc.left), (UINT32)(rc.bottom - rc.top)));
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_SIZE:
        if (g_target) {
            RECT rc; GetClientRect(hwnd, &rc);
            g_target->Resize(D2D1::SizeU((UINT32)(rc.right - rc.left), (UINT32)(rc.bottom - rc.top)));
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; BeginPaint(hwnd, &ps);
        Render();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_KEYDOWN:
        switch (wParam) {
        case VK_LEFT: case VK_UP: SetFocusIndex(g_focus - 1, true); return 0;
        case VK_RIGHT: case VK_DOWN: SetFocusIndex(g_focus + 1, true); return 0;
        case VK_RETURN: case VK_SPACE: LaunchEntry(g_focus); return 0;
        case VK_ESCAPE: return 0; // Never quit to desktop by accident.
        case '1': case '2': case '3': case '4': SetFocusIndex((int)(wParam - '1'), true); return 0;
        }
        return 0;
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
        int card = CardAtPoint(x, y);
        if (card >= 0) SetFocusIndex(card, true);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
        int card = CardAtPoint(x, y);
        if (card >= 0) { SetFocusIndex(card, false); LaunchEntry(card); }
        return 0;
    }
    case WM_POINTERDOWN: {
        // v1.0.4 touch fix: WM_POINTERDOWN lParam coordinates are
        // SCREEN coordinates, unlike WM_LBUTTONDOWN / WM_MOUSEMOVE
        // which are client coordinates. The old code fed screen
        // coordinates straight into CardAtPoint (client coordinates),
        // so a touch tap landed on the wrong card whenever the window
        // was not at (0,0) — e.g. windowed testing, or a DPI-scaled
        // layout. Convert first, then hit-test.
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &pt);
        int card = CardAtPoint(pt.x, pt.y);
        if (card >= 0) { SetFocusIndex(card, false); LaunchEntry(card); }
        return 0;
    }
    case WM_SHOW_SELECTOR:
        ShowWindow(hwnd, SW_SHOW);
        StartControllerTimer();
        SetForegroundWindow(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_DESTROY:
        StopControllerTimer();
        KillTimer(hwnd, TIMER_TRANSITION);
        g_desktopTransition = false;
        mciSendStringW(L"close rogselect_sound", nullptr, 0, nullptr);
        DiscardDeviceResources();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // v1.0.4 DPI fix: Per-Monitor V2, not the old system-DPI-only
    // SetProcessDPIAware(). The old call made Windows bitmap-scale the
    // window when the monitor DPI differed, so a window sized from
    // SM_CXSCREEN/SM_CYSCREEN could be scaled beyond the physical
    // screen at 175% and clip artwork/text on the right and bottom.
    // With Per-Monitor V2, SM_CXSCREEN is the true physical size,
    // GetClientRect is physical pixels, and layout (which is always
    // recomputed from the client rect) is identical at 100/150/175%
    // for the same physical resolution. Fallback: system-DPI aware.
    {
        typedef BOOL (WINAPI* PFN_SetDpiCtx)(DPI_AWARENESS_CONTEXT);
        HMODULE u32 = LoadLibraryW(L"user32.dll");
        bool v2 = false;
        if (u32) {
            auto fn = (PFN_SetDpiCtx)GetProcAddress(u32, "SetProcessDpiAwarenessContext");
            if (fn) v2 = fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE;
        }
        if (!v2) SetProcessDPIAware();
    }
    // v1.0.4 Unicode fix: window class, title, mutex and message box
    // are now all W APIs with a wide literal title. The old code mixed
    // ANSI window APIs (RegisterClassA / CreateWindowExA) into a build
    // that defines UNICODE, so the title passed through the ANSI
    // codepage conversion — the garbled-title symptom Codex reported.
    // Exe path and command line are also read as wide and converted
    // to UTF-8 (the internal string encoding, matching ToWide/CP_UTF8),
    // so a non-ASCII user path no longer garbles asset/config paths.
    wchar_t exePathW[MAX_PATH] = { 0 };
    GetModuleFileNameW(nullptr, exePathW, MAX_PATH);
    std::string full = WideToUtf8(exePathW);
    size_t slash = full.find_last_of("\\/");
    g_exeDir = (slash == std::string::npos) ? "." : full.substr(0, slash);
    g_configPath = g_exeDir + "\\selector.ini";
    // Command line: --windowed for safe testing, --config=<path> to override.
    std::string cmd = WideToUtf8(GetCommandLineW());
    if (cmd.find("--windowed") != std::string::npos) g_windowed = true;
    size_t cp = cmd.find("--config=");
    if (cp != std::string::npos) {
        size_t start = cp + 9, end = cmd.find(' ', start);
        g_configPath = cmd.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (g_configPath.size() >= 2 && g_configPath.front() == '"' && g_configPath.back() == '"')
            g_configPath = g_configPath.substr(1, g_configPath.size() - 2);
    }
    Log("ROG Select starting");
    LoadConfig();
    LogFseState("startup");
    // Single instance: a second launch shows the existing window.
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"ROGSelectSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = FindWindowW(L"ROGSelectWindow", nullptr);
        if (other) PostMessage(other, WM_SHOW_SELECTOR, 0, 0);
        return 0;
    }
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_factory);
    if (FAILED(hr)) { MessageBoxW(nullptr, L"Direct2D could not start. See rog-select.log.", L"ROG Select", MB_OK | MB_ICONERROR); return 1; }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_wic));
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&g_write);
    if (!g_wic) Log("WIC factory failed - images will use fallback cards");

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.lpszClassName = L"ROGSelectWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassW(&wc);
    DWORD style = g_windowed ? WS_OVERLAPPEDWINDOW : WS_POPUP;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    int w = g_windowed ? 1280 : sw, h = g_windowed ? 720 : sh;
    int x = g_windowed ? (sw - w) / 2 : 0, y = g_windowed ? (sh - h) / 2 : 0;
    g_hwnd = CreateWindowExW(0, L"ROGSelectWindow", L"ROG Select", style, x, y, w, h, nullptr, nullptr, hInst, nullptr);
    if (!g_hwnd) { Log("CreateWindow failed"); return 1; }
    SetWindowTextW(g_hwnd, L"ROG Select");
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    // Blocking message loop: effectively zero CPU when idle. The 80 ms
    // controller timer is the only wake-up, and painting happens only on
    // focus change, resize, launch, or return.
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    if (g_factory) g_factory->Release();
    if (g_wic) g_wic->Release();
    if (g_write) g_write->Release();
    if (mutex) CloseHandle(mutex);
    Log("ROG Select exiting");
    return (int)msg.wParam;
}
