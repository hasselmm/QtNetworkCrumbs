/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2023 Mathias Hasselmann
 */
#include "qncliterals.h"

namespace qnc {
static_assert('*'_L1.unicode() == 42);
static_assert("constexpr"_L1.size() == 9);
} // namespace qnc
