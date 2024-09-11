/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2023 Mathias Hasselmann
 */
#ifndef QNC_QNCLITERALS_H
#define QNC_QNCLITERALS_H

#include <QDateTime>
#include <QString>
#include <QUrl>

namespace qnc {
namespace literals {
namespace compat {

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)

using ByteArrayView = QByteArrayView;
using lentype = qsizetype;

#else // QT_VERSION < QT_VERSION_CHECK(6,0,0)

using ByteArrayView = QLatin1String;
using lentype = int;

#endif // QT_VERSION < QT_VERSION_CHECK(6,0,0)

} // namespace compat

#if QT_VERSION >= QT_VERSION_CHECK(6,4,0)

using namespace Qt::StringLiterals;

#elif QT_VERSION >= QT_VERSION_CHECK(6,0,0)

constexpr auto operator ""_L1(const char ch)
{
    return QLatin1Char{ch};
}

constexpr auto operator ""_L1(const char *str, size_t len)
{
    return QLatin1String{str, static_cast<qsizetype>(len)};
}

#else // QT_VERSION < QT_VERSION_CHECK(6,4,0)

constexpr auto operator ""_L1(const char ch)
{
    return QLatin1Char{ch};
}

constexpr auto operator ""_L1(const char *str, size_t len)
{
    return QLatin1String{str, static_cast<compat::lentype>(len)};
}

#endif // QT_VERSION < QT_VERSION_CHECK(6,4,0)

constexpr auto operator ""_baview(const char *str, size_t len)
{
    return compat::ByteArrayView{str, static_cast<compat::lentype>(len)};
}

inline auto operator ""_iso8601(const char *str, size_t len)
{
    return QDateTime::fromString(QLatin1String{str, static_cast<compat::lentype>(len)}, Qt::ISODate);
}

inline auto operator ""_url(const char *str, size_t len)
{
    return QUrl::fromEncoded({str, static_cast<compat::lentype>(len)});
}

} // namespace literals

using namespace literals;

} // namespace qnc

#endif // QNC_QNCLITERALS_H
