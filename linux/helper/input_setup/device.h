#pragma once

#include <string>
#include <tuple>

namespace input_setup {
struct Device {
    std::string vendor, product, type;
    bool operator<(const Device &other) const {
        return std::tie(vendor, product, type) < std::tie(other.vendor, other.product, other.type);
    }
};
inline const char *flag(const std::string &type) {
    if (type == "keyboard") return "ID_INPUT_KEY";
    if (type == "mouse") return "ID_INPUT_MOUSE";
    if (type == "gamepad") return "ID_INPUT_JOYSTICK";
    return nullptr;
}
inline bool hex(const std::string &s) {
    if (s.size() != 4) return false;
    for (char c : s) if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f')) return false;
    return true;
}
inline bool parseDevice(const std::string &s, Device &d) {
    if (s.size() < 11 || s[4] != ':' || s[9] != ':') return false;
    d = {s.substr(0, 4), s.substr(5, 4), s.substr(10)};
    return hex(d.vendor) && hex(d.product) && flag(d.type);
}
}
