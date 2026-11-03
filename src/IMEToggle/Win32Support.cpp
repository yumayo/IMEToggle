#include "Win32Support.h"

#include <array>
#include <stdexcept>
#include <vector>

namespace imetoggle {
namespace {
constexpr wchar_t startup_key[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t startup_name[] = L"IMEToggle";

std::wstring quoted_path(const std::wstring& path) {
    return L"\"" + path + L"\"";
}
} // namespace

std::wstring executable_path() {
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        throw std::runtime_error("Cannot determine executable path");
    }
    return std::wstring(buffer.data(), length);
}

void show_error(HWND owner, const wchar_t* message, DWORD error) {
    std::wstring text(message);
    if (error != ERROR_SUCCESS) {
        text += L"\n\nWindowsエラー: " + std::to_wstring(error);
        std::array<wchar_t, 1024> description{};
        if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, error, 0, description.data(), static_cast<DWORD>(description.size()), nullptr) != 0) {
            text += L"\n";
            text += description.data();
        }
    }
    MessageBoxW(owner, text.c_str(), L"IMEToggle", MB_OK | MB_ICONERROR);
}

HICON create_tray_icon(bool enabled) noexcept {
    HDC screen = GetDC(nullptr);
    if (screen == nullptr) {
        return nullptr;
    }
    HDC memory = CreateCompatibleDC(screen);
    BITMAPINFO info{};
    info.bmiHeader.biSize = static_cast<DWORD>(sizeof(BITMAPINFOHEADER));
    info.bmiHeader.biWidth = 32;
    info.bmiHeader.biHeight = -32;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP color = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    ReleaseDC(nullptr, screen);
    std::array<BYTE, 128> mask_bits{};
    HBITMAP mask = CreateBitmap(32, 32, 1, 1, mask_bits.data());
    HICON icon = nullptr;
    if (memory != nullptr && color != nullptr && mask != nullptr && pixels != nullptr) {
        HGDIOBJ previous_bitmap = SelectObject(memory, color);
        RECT rectangle{0, 0, 32, 32};
        HBRUSH brush = CreateSolidBrush(enabled ? RGB(0, 110, 85) : RGB(105, 105, 105));
        FillRect(memory, &rectangle, brush != nullptr ? brush : GetSysColorBrush(COLOR_WINDOW));
        if (brush != nullptr) {
            DeleteObject(brush);
        }
        SetBkMode(memory, TRANSPARENT);
        SetTextColor(memory, RGB(255, 255, 255));
        HFONT font = CreateFontW(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Yu Gothic UI");
        HGDIOBJ previous_font = font != nullptr ? SelectObject(memory, font) : nullptr;
        DrawTextW(memory, L"あ", -1, &rectangle, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (font != nullptr) {
            SelectObject(memory, previous_font);
            DeleteObject(font);
        }
        GdiFlush();
        auto* bitmap_pixels = static_cast<DWORD*>(pixels);
        for (std::size_t i = 0; i < 32 * 32; ++i) {
            bitmap_pixels[i] |= 0xFF000000u;
        }
        SelectObject(memory, previous_bitmap);
        ICONINFO icon_info{};
        icon_info.fIcon = TRUE;
        icon_info.hbmColor = color;
        icon_info.hbmMask = mask;
        icon = CreateIconIndirect(&icon_info);
    }
    if (memory != nullptr) {
        DeleteDC(memory);
    }
    if (color != nullptr) {
        DeleteObject(color);
    }
    if (mask != nullptr) {
        DeleteObject(mask);
    }
    return icon;
}

StartupState read_startup(const std::wstring& executable) {
    DWORD bytes = 0;
    LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, startup_key, startup_name,
        RRF_RT_REG_SZ, nullptr, nullptr, &bytes);
    if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) {
        return {false, ERROR_SUCCESS};
    }
    if (status != ERROR_SUCCESS) {
        return {false, status};
    }
    // Bound registry data before allocating. Executable paths cannot be longer.
    if (bytes < sizeof(wchar_t) || bytes > 131072 || bytes % sizeof(wchar_t) != 0) {
        return {false, ERROR_INVALID_DATA};
    }
    std::vector<wchar_t> value(bytes / sizeof(wchar_t), L'\0');
    status = RegGetValueW(HKEY_CURRENT_USER, startup_key, startup_name,
        RRF_RT_REG_SZ, nullptr, value.data(), &bytes);
    if (status != ERROR_SUCCESS) {
        return {false, status};
    }
    const std::wstring expected = quoted_path(executable);
    return {CompareStringOrdinal(value.data(), -1, expected.c_str(), -1, TRUE) == CSTR_EQUAL, ERROR_SUCCESS};
}

LSTATUS set_startup(const std::wstring& executable, bool enabled) {
    // Allocate before opening the handle, so exceptions cannot leak the key.
    const std::wstring command = quoted_path(executable);
    HKEY key = nullptr;
    LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, startup_key, 0, nullptr, 0,
        KEY_SET_VALUE, nullptr, &key, nullptr);
    if (status != ERROR_SUCCESS) {
        return status;
    }
    if (enabled) {
        status = RegSetValueExW(key, startup_name, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        status = RegDeleteValueW(key, startup_name);
        if (status == ERROR_FILE_NOT_FOUND) {
            status = ERROR_SUCCESS;
        }
    }
    RegCloseKey(key);
    return status;
}

} // namespace imetoggle
