#pragma once
#include "../core/input_types.h"

namespace Shadowless::Input::Evdev {
// Internal translation boundary; unknown codes never become identifiers.
InputId keyId(unsigned int code);
}
