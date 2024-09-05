/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2023 Mathias Hasselmann
 */
#ifndef QNC_QNCLITERALS_H
#define QNC_QNCLITERALS_H

#include <QString>

namespace qnc {
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

} // namespace qnc

#endif // QNC_QNCLITERALS_H
