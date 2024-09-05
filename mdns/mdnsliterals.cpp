#include "mdnsliterals.h"

namespace MDNS {
static_assert('*'_L1.unicode() == 42);
static_assert("constexpr"_L1.size() == 9);
} // namespace MDNS
