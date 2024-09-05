/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2023 Mathias Hasselmann
 */
#ifndef MDNS_MDNSLITERALS_H
#define MDNS_MDNSLITERALS_H

#include <QString>

namespace MDNS {
namespace literals {

#if QT_VERSION >= QT_VERSION_CHECK(6,4,0)

using namespace Qt::StringLiterals;

#else // QT_VERSION_CHECK(6,4,0)

constexpr auto operator ""_L1(const char ch)
{
    return QLatin1Char{ch};
}

constexpr auto operator ""_L1(const char *str, size_t len)
{
    return QLatin1String{str, static_cast<int>(len)};
}

#endif // QT_VERSION_CHECK(6,4,0)

} // namespace literals

using namespace literals;

} // namespace MDNS

#endif // MDNS_MDNSLITERALS_H
