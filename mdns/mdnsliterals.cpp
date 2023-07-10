#include "mdnsliterals.h"

namespace MDNS {
static_assert('*'_l1.unicode() == 42);
static_assert("constexpr"_l1.size() == 9);
} // namespace MDNS
