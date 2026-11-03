#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace imetoggle {

class KeyRemapper {
public:
    static constexpr std::uint32_t f13 = 0x7C;
    static constexpr std::uint32_t f14 = 0x7D;
    static constexpr std::uint16_t ime_off = 0x1A;
    static constexpr std::uint16_t ime_on = 0x16;
    using SendKey = bool (*)(std::uint16_t, void*) noexcept;

    explicit KeyRemapper(SendKey sender, void* context = nullptr) noexcept
        : sender_(sender), context_(context) {}

    // A press is classified once. Repeats and release keep that classification
    // even if modifiers, pause status or injection permissions change meanwhile.
    bool handle_key(std::uint32_t key, bool key_up, bool modifiers, bool enabled) noexcept {
        if (key != f13 && key != f14) {
            return false;
        }

        State& state = pressed_[key == f13 ? 0 : 1];
        if (key_up) {
            const bool consumed = state == State::consumed;
            state = State::released;
            return consumed;
        }
        if (state != State::released) {
            return state == State::consumed;
        }

        const bool consumed = enabled && !modifiers && sender_ != nullptr
            && sender_(key == f13 ? ime_off : ime_on, context_);
        state = consumed ? State::consumed : State::passed;
        return consumed;
    }

private:
    enum class State { released, passed, consumed };
    std::array<State, 2> pressed_{};
    SendKey sender_;
    void* context_;
};

} // namespace imetoggle
