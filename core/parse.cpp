/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#include "parse.h"

// STL headers
#include <cmath>

#if QT_VERSION_MAJOR < 6
using QByteArrayView = QByteArray;
#endif // QT_VERSION_MAJOR < 6

namespace qnc::core::detail {

template <>
void parse(const QByteArrayView &text, bool &value, bool &isValid)
{
    const auto &trimmedText = text.trimmed();

    if (trimmedText.compare("true", Qt::CaseInsensitive) == 0
            || trimmedText.compare("yes", Qt::CaseInsensitive) == 0
            || trimmedText.compare("on", Qt::CaseInsensitive) == 0
            || trimmedText.compare("enabled", Qt::CaseInsensitive) == 0) {
        value = true;
        isValid = true;
    } else if (trimmedText.compare("false", Qt::CaseInsensitive) == 0
               || trimmedText.compare("no", Qt::CaseInsensitive) == 0
               || trimmedText.compare("off", Qt::CaseInsensitive) == 0
               || trimmedText.compare("disabled", Qt::CaseInsensitive) == 0) {
        value = false;
        isValid = true;
    } else if (const auto &number = core::parse<int>(trimmedText)) {
        value = (number != 0);
        isValid = true;
    } else {
        isValid = false;
    }
}

template <>
void parse(const QByteArrayView &text, long double &value, bool &isValid)
{
    if (Q_UNLIKELY(text.compare("+nan", Qt::CaseInsensitive) == 0)
            || Q_UNLIKELY(text.compare("-nan", Qt::CaseInsensitive) == 0)) {
        isValid = false; // reject nan with sign to align with QString::toFloat() and ::toDouble()
    } else if (Q_UNLIKELY(text.compare("+inf", Qt::CaseInsensitive) == 0)
               || Q_UNLIKELY(text.compare("inf", Qt::CaseInsensitive) == 0)) {
        value = qInf(); // parse infinitity early so that it can be distinguished from out-of-range error
        isValid = true;
    } else if (Q_UNLIKELY(text.compare("-inf", Qt::CaseInsensitive) == 0)) {
        value = -qInf(); // parse infinitity early so that it can be distinguished from out-of-range error
        isValid = true;
    } else {
        const auto first = text.constData();
        auto last = static_cast<char *>(nullptr);
        const auto result = value = std::strtold(first, &last);

        if (Q_UNLIKELY(last == nullptr)
                || Q_UNLIKELY(last == first)
                || Q_UNLIKELY(last[0] != '\0')
                || Q_UNLIKELY(std::isinf(result))) {
            isValid = false;
        } else {
            value = result;
            isValid = true;
        }
    }
}

#if QT_VERSION_MAJOR < 6

template <> void parse(const QLatin1String   &text, long double &value, bool &isValid) { parse(QByteArray{text.data(), text.size()}, value, isValid); }
template <> void parse(const QLatin1String   &text, bool        &value, bool &isValid) { parse(QByteArray{text.data(), text.size()}, value, isValid); }

#else // QT_VERSION_MAJOR >= 6

template <> void parse(const QByteArray      &text, long double &value, bool &isValid) { parse(QByteArrayView{text}, value, isValid); }
template <> void parse(const QByteArray      &text, bool        &value, bool &isValid) { parse(QByteArrayView{text}, value, isValid); }
template <> void parse(const QLatin1String   &text, long double &value, bool &isValid) { parse(QByteArrayView{text}, value, isValid); }
template <> void parse(const QLatin1String   &text, bool        &value, bool &isValid) { parse(QByteArrayView{text}, value, isValid); }

#endif // QT_VERSION_MAJOR >= 6

template <> void parse(const QStringView     &text, long double &value, bool &isValid) { parse(text.toLatin1(), value, isValid); }
template <> void parse(const QStringView     &text, bool        &value, bool &isValid) { parse(text.toLatin1(), value, isValid); }
template <> void parse(const QString         &text, long double &value, bool &isValid) { parse(text.toLatin1(), value, isValid); }
template <> void parse(const QString         &text, bool        &value, bool &isValid) { parse(text.toLatin1(), value, isValid); }

#if QT_VERSION_MAJOR >= 6

template <> void parse(const QAnyStringView  &text, long double &value, bool &isValid) { parse(text.toString(), value, isValid); }
template <> void parse(const QAnyStringView  &text, bool        &value, bool &isValid) { parse(text.toString(), value, isValid); }
template <> void parse(const QUtf8StringView &text, long double &value, bool &isValid) { parse(text.toString(), value, isValid); }
template <> void parse(const QUtf8StringView &text, bool        &value, bool &isValid) { parse(text.toString(), value, isValid); }

#endif // QT_VERSION_MAJOR >= 6

} // namespace qnc::core::detail
