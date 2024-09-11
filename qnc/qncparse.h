/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2023 Mathias Hasselmann
 */
#ifndef QNC_QNCPARSE_H
#define QNC_QNCPARSE_H

// Qt headers
#include <QString>

// STL headers
#include <optional>

namespace qnc {

namespace detail {

template <class S, typename T> auto parseMethod(const T &) = delete;
template <class S>             auto parseMethod(const short      &) { return &S::toShort;     }
template <class S>             auto parseMethod(const ushort     &) { return &S::toUShort;    }
template <class S>             auto parseMethod(const int        &) { return &S::toInt;       }
template <class S>             auto parseMethod(const uint       &) { return &S::toUInt;      }
template <class S>             auto parseMethod(const long       &) { return &S::toLong;      }
template <class S>             auto parseMethod(const ulong      &) { return &S::toULong;     }
template <class S>             auto parseMethod(const qlonglong  &) { return &S::toLongLong;  }
template <class S>             auto parseMethod(const qulonglong &) { return &S::toULongLong; }
template <class S>             auto parseMethod(const float      &) { return &S::toFloat;     }
template <class S>             auto parseMethod(const double     &) { return &S::toDouble;    }

template <class S, typename T>
using HasBase = std::is_same<T (S::* )(bool *, int) const, decltype(detail::parseMethod<S>(T{}))>;

template <typename T, class StringLike, typename ...Args>
std::optional<T> parse(const StringLike &text, Args &&...args)
{
    auto isValid = false;
    const auto parser = detail::parseMethod<StringLike>(T{});
    const auto value = (text.*parser)(&isValid, std::forward<Args>(args)...);

    if (Q_LIKELY(isValid))
        return value;

    return {};
}

} // namespace detail

template <typename T, class StringLike, typename = std::enable_if_t<detail::HasBase<StringLike, T>::value>>
std::optional<T> parse(const StringLike &text, int base = 10) { return detail::parse<T>(text, base); }

template <typename T, class StringLike, typename = std::enable_if_t<!detail::HasBase<StringLike, T>::value>>
std::optional<T> parse(const StringLike &text) { return detail::parse<T>(text); }

} // namespace qnc

#endif // QNC_QNCPARSE_H
