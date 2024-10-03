/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#include "literals.h"

namespace qnc::core {
static_assert('*'_L1.unicode() == 42);
static_assert("constexpr"_L1.size() == 9);
} // namespace qnc::core
