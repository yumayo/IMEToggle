#pragma once

#include <windows.h>
#include <string>
#include <string_view>

namespace imetoggle {

template<std::size_t Size>
void copy_text(wchar_t (&destination)[Size], std::wstring_view source) noexcept {
    static_assert(Size > 0);
    const std::size_t count = source.size() < Size - 1 ? source.size() : Size - 1;
    for (std::size_t i = 0; i < count; ++i) {
        destination[i] = source[i];
    }
    destination[count] = L'\0';
}

std::wstring executable_path();
void show_error(HWND owner, const wchar_t* message, DWORD error);
HICON create_tray_icon(bool enabled, UINT dpi) noexcept;

struct StartupState {
    bool enabled;
    LSTATUS error;
};

StartupState read_startup(const std::wstring& executable);
LSTATUS set_startup(const std::wstring& executable, bool enabled);

} // namespace imetoggle
