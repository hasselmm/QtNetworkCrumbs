/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2023 Mathias Hasselmann
 */
#ifndef MDNS_MDNSLITERALS_H
#define MDNS_MDNSLITERALS_H

#include <QString>

namespace MDNS {
namespace literals {

constexpr auto operator ""_l1(const char ch)
{
    return QLatin1Char{ch};
}

constexpr auto operator ""_l1(const char *str, size_t len)
{
    return QLatin1String{str, static_cast<int>(len)};
}

} // namespace literals

using namespace literals;

} // namespace MDNS

#endif // MDNS_MDNSLITERALS_H
