/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2023 Mathias Hasselmann
 */
#ifndef QNC_QNCPARSE_H
#define QNC_QNCPARSE_H

// Qt headers
#include <QString>

// STL headers
#include <limits>
#include <optional>

namespace qnc {

namespace detail {

template <class S, typename T>
void parseIndirect(const S &text, T &value, bool &isValid, int base)
{
    auto genericValue = text.toInt(&isValid, base);

    if (Q_LIKELY(isValid)) {
        if (Q_LIKELY(genericValue >= std::numeric_limits<T>::min())
                && Q_LIKELY(genericValue <= std::numeric_limits<T>::max()))
            value = static_cast<T>(genericValue);
        else
            isValid = false;
    }
}

template <class S, typename T, typename ...Args>
void parse(const S &, T &, bool &, Args...) = delete;

template <class S> void parse(const S &text, bool        &value, bool &isValid);
template <class S> void parse(const S &text, qint8       &value, bool &isValid, int base = 10) { parseIndirect(text, value, isValid, base); }
template <class S> void parse(const S &text, quint8      &value, bool &isValid, int base = 10) { parseIndirect(text, value, isValid, base); }
template <class S> void parse(const S &text, short       &value, bool &isValid, int base = 10) { value = text.toShort     (&isValid, base); }
template <class S> void parse(const S &text, ushort      &value, bool &isValid, int base = 10) { value = text.toUShort    (&isValid, base); }
template <class S> void parse(const S &text, int         &value, bool &isValid, int base = 10) { value = text.toInt       (&isValid, base); }
template <class S> void parse(const S &text, uint        &value, bool &isValid, int base = 10) { value = text.toUInt      (&isValid, base); }
template <class S> void parse(const S &text, long        &value, bool &isValid, int base = 10) { value = text.toLong      (&isValid, base); }
template <class S> void parse(const S &text, ulong       &value, bool &isValid, int base = 10) { value = text.toULong     (&isValid, base); }
template <class S> void parse(const S &text, qlonglong   &value, bool &isValid, int base = 10) { value = text.toLongLong  (&isValid, base); }
template <class S> void parse(const S &text, qulonglong  &value, bool &isValid, int base = 10) { value = text.toULongLong (&isValid, base); }
template <class S> void parse(const S &text, float       &value, bool &isValid)                { value = text.toFloat     (&isValid); }
template <class S> void parse(const S &text, double      &value, bool &isValid)                { value = text.toDouble    (&isValid); }
template <class S> void parse(const S &text, long double &value, bool &isValid);

template <typename T, class StringLike, typename ...Args>
std::optional<T> parse(const StringLike &text, Args &&...args)
{
    auto value = T{};
    auto isValid = false;

    parse(text, value, isValid, std::forward<Args>(args)...);

    if (Q_LIKELY(isValid))
        return value;

    return {};
}

template <typename T, typename Char, std::size_t N, typename ...Args>
std::optional<T> parse(const Char (&str)[N], Args &&...args)
{
    const auto it = std::char_traits<Char>::find(str, N, Char(0));
    const auto end = it ? it : std::end(str);

    return parse<T>(QStringView{str, end}, std::forward<Args>(args)...);
}

} // namespace detail

template <typename T, class StringLike>
std::optional<T> parse(const StringLike &text) { return detail::parse<T>(text); }

template <typename T, class StringLike>
std::optional<T> parse(const StringLike &text, int base) { return detail::parse<T>(text, base); }

} // namespace qnc

#endif // QNC_QNCPARSE_H
