#include "KeyRemapper.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using imetoggle::KeyRemapper;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct Sender {
    std::array<std::uint16_t, 16> keys{};
    std::size_t attempts = 0;
    std::size_t accepted = 0;
    bool succeeds = true;

    static bool send(std::uint16_t key, void* context) noexcept {
        auto& self = *static_cast<Sender*>(context);
        ++self.attempts;
        if (!self.succeeds || self.accepted >= self.keys.size()) {
            return false;
        }
        self.keys[self.accepted++] = key;
        return true;
    }
};

void explicit_state() {
    Sender sender;
    KeyRemapper mapper(Sender::send, &sender);
    for (int i = 0; i < 2; ++i) {
        require(mapper.handle_key(0x7C, false, false, true), "F13 down");
        require(mapper.handle_key(0x7C, true, false, true), "F13 up");
        require(mapper.handle_key(0x7D, false, false, true), "F14 down");
        require(mapper.handle_key(0x7D, true, false, true), "F14 up");
    }
    require(sender.accepted == 4, "Each press must send exactly one command");
    require(sender.keys[0] == 0x1A && sender.keys[1] == 0x16
        && sender.keys[2] == 0x1A && sender.keys[3] == 0x16, "IME OFF and ON must not be toggles");
}

void repeats() {
    Sender sender;
    KeyRemapper mapper(Sender::send, &sender);
    require(mapper.handle_key(KeyRemapper::f14, false, false, true), "Initial down");
    require(mapper.handle_key(KeyRemapper::f14, false, true, true), "Repeat after modifier change");
    require(mapper.handle_key(KeyRemapper::f14, false, false, true), "Repeat");
    require(mapper.handle_key(KeyRemapper::f14, true, true, true), "Up after modifier change");
    require(sender.accepted == 1, "Holding a key must not repeatedly inject input");
    require(mapper.handle_key(KeyRemapper::f14, false, false, true), "New down after release");
    require(sender.accepted == 2, "Fresh press must send another command");
}

void modifiers() {
    Sender sender;
    KeyRemapper mapper(Sender::send, &sender);
    require(!mapper.handle_key(KeyRemapper::f13, false, true, true), "Modified down must pass");
    require(!mapper.handle_key(KeyRemapper::f13, false, false, true), "Repeat after modifier release must pass");
    require(!mapper.handle_key(KeyRemapper::f13, true, false, true), "Modified press release must pass");
    require(sender.attempts == 0, "Modified presses must not inject IME keys");
    require(mapper.handle_key(KeyRemapper::f13, false, false, true), "Later unmodified press must work");
}

void pause() {
    Sender sender;
    KeyRemapper mapper(Sender::send, &sender);
    require(mapper.handle_key(KeyRemapper::f13, false, false, true), "Down before pause");
    require(mapper.handle_key(KeyRemapper::f13, false, false, false), "Held repeat during pause");
    require(mapper.handle_key(KeyRemapper::f13, true, false, false), "Consumed release during pause");
    require(!mapper.handle_key(KeyRemapper::f13, false, false, false), "New down during pause must pass");
    require(!mapper.handle_key(KeyRemapper::f13, true, false, false), "Paused release must pass");
    require(sender.attempts == 1, "Pausing must suppress new injections");
}

void resume() {
    Sender sender;
    KeyRemapper mapper(Sender::send, &sender);
    require(!mapper.handle_key(KeyRemapper::f14, false, false, false), "Down while paused");
    require(!mapper.handle_key(KeyRemapper::f14, false, false, true), "Held repeat after resuming");
    require(!mapper.handle_key(KeyRemapper::f14, true, false, true), "Up after resuming");
    require(sender.attempts == 0, "Resuming must not capture an existing press");
    require(mapper.handle_key(KeyRemapper::f14, false, false, true), "Fresh press after resuming");
}

void injection_failure() {
    Sender sender;
    sender.succeeds = false;
    KeyRemapper mapper(Sender::send, &sender);
    require(!mapper.handle_key(KeyRemapper::f13, false, false, true), "Failed down must pass");
    sender.succeeds = true;
    require(!mapper.handle_key(KeyRemapper::f13, false, false, true), "Repeat after failure must pass");
    require(!mapper.handle_key(KeyRemapper::f13, true, false, true), "Failed press release must pass");
    require(sender.attempts == 1, "Do not retry during a held press");
    require(mapper.handle_key(KeyRemapper::f13, false, false, true), "A fresh press must retry");
    require(sender.attempts == 2 && sender.accepted == 1, "Recovery after injection failure");
}

void unrelated() {
    Sender sender;
    KeyRemapper mapper(Sender::send, &sender);
    for (std::uint32_t key = 0; key < 256; ++key) {
        if (key != KeyRemapper::f13 && key != KeyRemapper::f14) {
            require(!mapper.handle_key(key, false, false, true), "Unrelated down must pass");
            require(!mapper.handle_key(key, true, false, true), "Unrelated up must pass");
        }
    }
    require(!mapper.handle_key(KeyRemapper::f13, true, false, true), "Unmatched F13 up");
    require(!mapper.handle_key(KeyRemapper::f14, true, false, true), "Unmatched F14 up");
    require(sender.attempts == 0, "Unrelated input must not inject anything");
}

void overlap() {
    Sender sender;
    KeyRemapper mapper(Sender::send, &sender);
    require(mapper.handle_key(KeyRemapper::f13, false, false, true), "Overlapping F13 down");
    require(mapper.handle_key(KeyRemapper::f14, false, false, true), "Overlapping F14 down");
    require(mapper.handle_key(KeyRemapper::f13, false, false, true), "Overlapping F13 repeat");
    require(mapper.handle_key(KeyRemapper::f13, true, false, true), "Overlapping F13 up");
    require(mapper.handle_key(KeyRemapper::f14, true, false, true), "Overlapping F14 up");
    require(sender.accepted == 2 && sender.keys[0] == 0x1A && sender.keys[1] == 0x16, "Overlapping key order");
}

void no_sender() {
    KeyRemapper mapper(nullptr);
    require(!mapper.handle_key(KeyRemapper::f13, false, false, true), "Missing sender down must pass");
    require(!mapper.handle_key(KeyRemapper::f13, true, false, true), "Missing sender up must pass");
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Expected one test name\n";
        return 1;
    }
    const std::string_view name(argv[1]);
    try {
        if (name == "explicit-state") { explicit_state(); }
        else if (name == "repeats") { repeats(); }
        else if (name == "modifiers") { modifiers(); }
        else if (name == "pause") { pause(); }
        else if (name == "resume") { resume(); }
        else if (name == "injection-failure") { injection_failure(); }
        else if (name == "unrelated") { unrelated(); }
        else if (name == "overlap") { overlap(); }
        else if (name == "no-sender") { no_sender(); }
        else { throw std::runtime_error("Unknown test name"); }
        std::cout << "PASS: " << name << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << name << ": " << error.what() << '\n';
        return 1;
    }
}
