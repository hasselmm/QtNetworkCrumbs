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

using lentype = int;

// This is not an efficient implementation of QByteArrayView.
// This is just some glue to make this code work with Qt5.
// Use this this library with Qt6 if performance matters.
class ByteArrayView
{
public:
    constexpr ByteArrayView() : m_str{} {}
    constexpr ByteArrayView(const char *str, int len) : m_str{str, len} {}
    ByteArrayView(const QByteArray &ba) : m_str{ba.constData(), ba.size()} {}

    QByteArray toByteArray() const { return {m_str.data(), m_str.size()}; }
    operator QByteArray() const { return toByteArray(); }

    constexpr qsizetype length() const { return m_str.size(); }
    constexpr qsizetype size() const { return m_str.size(); }

    const char *cbegin() const { return m_str.cbegin(); }
    const char *cend() const { return m_str.cend(); }
    const char *data() const { return m_str.data(); }

    int  toInt (bool *ok = nullptr, int base = 10) const { return toByteArray().toInt (ok, base); }
    uint toUInt(bool *ok = nullptr, int base = 10) const { return toByteArray().toUInt(ok, base); }

    int compare(const QByteArray &r, Qt::CaseSensitivity cs = Qt::CaseSensitive) const
    { return m_str.compare(QLatin1String{r.data(), r.size()}, cs); }

    int indexOf(char c, qsizetype from = 0) const
    { return m_str.indexOf(QChar::fromLatin1(c), static_cast<lentype>(from)); }

private:
    QLatin1String m_str;
};

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

inline auto operator ""_ba(const char *str, size_t len)
{
    return QByteArray{str, static_cast<compat::lentype>(len)};
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
