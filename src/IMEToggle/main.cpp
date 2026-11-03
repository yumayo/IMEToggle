#include "KeyRemapper.h"
#include "Win32Support.h"

#include <shellapi.h>
#include <windowsx.h>
#include <exception>

namespace imetoggle {
namespace {
constexpr wchar_t window_class[] = L"IMEToggle.TrayWindow";
constexpr wchar_t mutex_name[] = L"Local\\IMEToggle.557f6da0-bcd4-484c-880f-280624e83a58";
constexpr UINT tray_message = WM_APP + 1;
constexpr UINT_PTR error_timer = 1;
constexpr ULONG_PTR input_marker = 0x494D4554u;
constexpr UINT menu_pause = 100;
constexpr UINT menu_startup = 101;
constexpr UINT menu_help = 102;
constexpr UINT menu_exit = 103;

static_assert(KeyRemapper::f13 == VK_F13 && KeyRemapper::f14 == VK_F14);
static_assert(KeyRemapper::ime_off == VK_IME_OFF && KeyRemapper::ime_on == VK_IME_ON);
static_assert(sizeof(INPUT) == (sizeof(void*) == 8 ? 40 : 28));
static_assert(sizeof(KBDLLHOOKSTRUCT) == (sizeof(void*) == 8 ? 24 : 20));

class Application {
public:
    explicit Application(HINSTANCE instance)
        : instance_(instance), executable_(executable_path()), remapper_(send_key, this) {}

    ~Application() {
        stop();
        if (window_ != nullptr) {
            DestroyWindow(window_);
        }
        if (registered_) {
            UnregisterClassW(window_class, instance_);
        }
        if (enabled_icon_ != nullptr) {
            DestroyIcon(enabled_icon_);
        }
        if (paused_icon_ != nullptr) {
            DestroyIcon(paused_icon_);
        }
        active_ = nullptr;
    }

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool start() {
        enabled_icon_ = create_tray_icon(true);
        paused_icon_ = create_tray_icon(false);
        if (enabled_icon_ == nullptr || paused_icon_ == nullptr) {
            show_error(nullptr, L"トレイアイコンを作成できませんでした。", ERROR_NOT_ENOUGH_MEMORY);
            return false;
        }
        WNDCLASSEXW definition{};
        definition.cbSize = static_cast<UINT>(sizeof(definition));
        definition.lpfnWndProc = window_proc;
        definition.hInstance = instance_;
        definition.lpszClassName = window_class;
        if (RegisterClassExW(&definition) == 0) {
            show_error(nullptr, L"ウィンドウを登録できませんでした。", GetLastError());
            return false;
        }
        registered_ = true;
        taskbar_created_ = RegisterWindowMessageW(L"TaskbarCreated");
        if (taskbar_created_ == 0) {
            show_error(nullptr, L"トレイの通知を登録できませんでした。", GetLastError());
            return false;
        }
        // A hidden top-level window receives Explorer-restart and session messages.
        window_ = CreateWindowExW(WS_EX_TOOLWINDOW, window_class, L"IMEToggle", WS_OVERLAPPED,
            0, 0, 0, 0, nullptr, nullptr, instance_, this);
        if (window_ == nullptr) {
            show_error(nullptr, L"ウィンドウを作成できませんでした。", GetLastError());
            return false;
        }
        active_ = this;
        hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_proc, instance_, 0);
        if (hook_ == nullptr) {
            show_error(window_, L"キーボードフックを開始できませんでした。", GetLastError());
            return false;
        }
        // Explorer may still be initializing at login; the timer retries.
        add_tray_icon();
        if (SetTimer(window_, error_timer, 1000, nullptr) == 0) {
            show_error(window_, L"通知タイマーを開始できませんでした。", GetLastError());
            return false;
        }
        return true;
    }

    int run() {
        MSG message{};
        BOOL result = 0;
        while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (result == -1) {
            show_error(window_, L"メッセージを取得できませんでした。", GetLastError());
            return 1;
        }
        return static_cast<int>(message.wParam);
    }

private:
    static Application* active_;
    HINSTANCE instance_;
    std::wstring executable_;
    KeyRemapper remapper_;
    HWND window_ = nullptr;
    HHOOK hook_ = nullptr;
    HICON enabled_icon_ = nullptr;
    HICON paused_icon_ = nullptr;
    UINT taskbar_created_ = 0;
    bool registered_ = false;
    bool tray_added_ = false;
    bool enabled_ = true;
    bool error_pending_ = false;
    DWORD input_error_ = ERROR_SUCCESS;
    ULONGLONG last_notification_ = 0;

    void stop() noexcept {
        if (hook_ != nullptr) {
            UnhookWindowsHookEx(hook_);
            hook_ = nullptr;
        }
        if (window_ != nullptr) {
            KillTimer(window_, error_timer);
        }
        if (tray_added_) {
            NOTIFYICONDATAW icon{};
            icon.cbSize = static_cast<DWORD>(sizeof(icon));
            icon.hWnd = window_;
            icon.uID = 1;
            Shell_NotifyIconW(NIM_DELETE, &icon);
            tray_added_ = false;
        }
    }

    NOTIFYICONDATAW tray_data() const noexcept {
        NOTIFYICONDATAW icon{};
        icon.cbSize = static_cast<DWORD>(sizeof(icon));
        icon.hWnd = window_;
        icon.uID = 1;
        icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        icon.uCallbackMessage = tray_message;
        icon.hIcon = enabled_ ? enabled_icon_ : paused_icon_;
        copy_text(icon.szTip, enabled_ ? L"IMEToggle - F13: OFF / F14: ON" : L"IMEToggle - 一時停止中");
        return icon;
    }

    bool add_tray_icon() noexcept {
        NOTIFYICONDATAW icon = tray_data();
        tray_added_ = Shell_NotifyIconW(NIM_ADD, &icon) != FALSE;
        if (tray_added_) {
            icon.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &icon);
        }
        return tray_added_;
    }

    static bool has_modifiers() noexcept {
        for (int key : {VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN}) {
            if ((GetAsyncKeyState(key) & 0x8000) != 0) {
                return true;
            }
        }
        return false;
    }

    static LRESULT CALLBACK keyboard_proc(int code, WPARAM wparam, LPARAM lparam) noexcept {
        if (code == HC_ACTION && active_ != nullptr) {
            const bool key_up = wparam == WM_KEYUP || wparam == WM_SYSKEYUP;
            const bool key_down = wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN;
            if (key_up || key_down) {
                const auto& input = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(lparam);
                // Other remappers' injected F13/F14 are valid; only ignore our tag.
                if (input.dwExtraInfo != input_marker &&
                    (input.vkCode == KeyRemapper::f13 || input.vkCode == KeyRemapper::f14) &&
                    active_->remapper_.handle_key(input.vkCode, key_up, has_modifiers(), active_->enabled_)) {
                    return 1;
                }
            }
        }
        return CallNextHookEx(nullptr, code, wparam, lparam);
    }

    static bool send_key(std::uint16_t key, void* context) noexcept {
        auto& app = *static_cast<Application*>(context);
        INPUT inputs[2]{};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = key;
        inputs[0].ki.dwExtraInfo = input_marker;
        inputs[1] = inputs[0];
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SetLastError(ERROR_SUCCESS);
        const UINT inserted = SendInput(2, inputs, static_cast<int>(sizeof(INPUT)));
        if (inserted == 2) {
            return true;
        }
        app.input_error_ = GetLastError();
        app.error_pending_ = true;
        if (inserted == 1) {
            SendInput(1, &inputs[1], static_cast<int>(sizeof(INPUT)));
        }
        return false;
    }

    void check_errors() {
        if (!tray_added_) {
            add_tray_icon();
        }
        const ULONGLONG now = GetTickCount64();
        if (!error_pending_ || !tray_added_ || (last_notification_ != 0 && now - last_notification_ < 30000)) {
            return;
        }
        error_pending_ = false;
        last_notification_ = now;
        NOTIFYICONDATAW icon = tray_data();
        icon.uFlags = NIF_INFO;
        icon.dwInfoFlags = NIIF_WARNING;
        copy_text(icon.szInfoTitle, L"IMEToggle");
        std::wstring text = L"IMEキーを送信できませんでした。対象アプリが管理者権限の場合は、"
            L"IMEToggleも同じ権限で起動してください。";
        if (input_error_ != ERROR_SUCCESS) {
            text += L"\nWindowsエラー: " + std::to_wstring(input_error_);
        }
        copy_text(icon.szInfo, text);
        Shell_NotifyIconW(NIM_MODIFY, &icon);
    }

    void show_menu(POINT position) {
        const StartupState startup = read_startup(executable_);
        HMENU menu = CreatePopupMenu();
        if (menu == nullptr) {
            return;
        }
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"F13: IME OFF / F14: IME ON");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (enabled_ ? MF_UNCHECKED : MF_CHECKED), menu_pause, L"一時停止");
        AppendMenuW(menu, MF_STRING | (startup.enabled ? MF_CHECKED : MF_UNCHECKED)
            | (startup.error == ERROR_SUCCESS ? MF_ENABLED : MF_GRAYED), menu_startup, L"ログイン時に起動");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, menu_help, L"使い方");
        AppendMenuW(menu, MF_STRING, menu_exit, L"終了");
        if (position.x == -1 && position.y == -1) {
            GetCursorPos(&position);
        }
        SetForegroundWindow(window_);
        const UINT selected = static_cast<UINT>(TrackPopupMenu(menu,
            TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, position.x, position.y, 0, window_, nullptr));
        DestroyMenu(menu);
        PostMessageW(window_, WM_NULL, 0, 0);
        switch (selected) {
        case menu_pause: {
            enabled_ = !enabled_;
            NOTIFYICONDATAW icon = tray_data();
            Shell_NotifyIconW(NIM_MODIFY, &icon);
            break;
        }
        case menu_startup: {
            const LSTATUS status = set_startup(executable_, !startup.enabled);
            if (status != ERROR_SUCCESS) {
                show_error(window_, L"自動起動の設定を変更できませんでした。", static_cast<DWORD>(status));
            }
            break;
        }
        case menu_help:
            show_help();
            break;
        case menu_exit:
            DestroyWindow(window_);
            break;
        default:
            break;
        }
    }

    void show_help() const noexcept {
        MessageBoxW(window_,
            L"F13: IMEをOFF（英数入力）\nF14: IMEをON（日本語入力）\n\n"
            L"日本語のMicrosoft IMEを選択した状態で使用してください。\n"
            L"同じキーを続けて押してもON/OFFは反転しません。\n"
            L"Ctrl・Shift・Alt・Winと組み合わせたF13/F14はそのまま通します。\n\n"
            L"緑色のアイコンは有効、灰色は一時停止中を示します。\n"
            L"現在のIMEのON/OFF状態を示すものではありません。\n\n"
            L"右クリックで一時停止・自動起動・終了を選べます。",
            L"IMEToggle の使い方", MB_OK | MB_ICONINFORMATION);
    }

    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
        auto* app = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            app = static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
            app->window_ = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }
        if (app != nullptr) {
            try {
                if (message == app->taskbar_created_) {
                    app->tray_added_ = false;
                    app->add_tray_icon();
                    return 0;
                }
                switch (message) {
                case tray_message:
                    if (LOWORD(lparam) == WM_CONTEXTMENU) {
                        app->show_menu(POINT{GET_X_LPARAM(wparam), GET_Y_LPARAM(wparam)});
                    } else if (LOWORD(lparam) == WM_LBUTTONDBLCLK) {
                        app->show_help();
                    }
                    return 0;
                case WM_TIMER:
                    if (wparam == error_timer) {
                        app->check_errors();
                    }
                    return 0;
                case WM_QUERYENDSESSION:
                    return TRUE;
                case WM_ENDSESSION:
                    if (wparam != 0) {
                        DestroyWindow(window);
                    }
                    return 0;
                case WM_CLOSE:
                    DestroyWindow(window);
                    return 0;
                case WM_DESTROY:
                    app->stop();
                    app->window_ = nullptr;
                    PostQuitMessage(0);
                    return 0;
                default:
                    break;
                }
            } catch (...) {
                // No C++ exception may escape a Win32 callback.
                MessageBoxW(window, L"処理を続けられないため終了します。", L"IMEToggle", MB_OK | MB_ICONERROR);
                app->stop();
                PostQuitMessage(1);
                return 0;
            }
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }
};

Application* Application::active_ = nullptr;
} // namespace
} // namespace imetoggle

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    HANDLE mutex = CreateMutexW(nullptr, FALSE, imetoggle::mutex_name);
    const DWORD mutex_error = GetLastError();
    if (mutex == nullptr) {
        imetoggle::show_error(nullptr, L"二重起動の確認に失敗しました。", mutex_error);
        return 1;
    }
    int result = 0;
    if (mutex_error == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"IMEToggleはすでに起動しています。タスクトレイを確認してください。",
            L"IMEToggle", MB_OK | MB_ICONINFORMATION);
    } else {
        try {
            imetoggle::Application application(instance);
            result = application.start() ? application.run() : 1;
        } catch (const std::exception&) {
            MessageBoxW(nullptr, L"IMEToggleを起動できませんでした。", L"IMEToggle", MB_OK | MB_ICONERROR);
            result = 1;
        }
    }
    CloseHandle(mutex);
    return result;
}
