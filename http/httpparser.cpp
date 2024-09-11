/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#include "httpparser.h"

// QtNetworkCrumbs headers
#include "qncliterals.h"
#include "qncparse.h"

// Qt headers
#include <QBuffer>
#include <QDateTime>
#include <QLoggingCategory>

namespace qnc::http {

namespace {

Q_LOGGING_CATEGORY(lcHttpParser, "qnc.http.parser")

// examples from https://www.rfc-editor.org/rfc/rfc9110#section-5.6.7
constexpr auto s_rfc1123DateFormat = "ddd, dd MMM yyyy hh:mm:ss 'GMT'"_L1;  // e.g. "Sun, 06 Nov 1994 08:49:37 GMT"
constexpr auto s_rfc850DateFormat = "dddd, dd-MMM-yy hh:mm:ss 'GMT'"_L1;    // e.g. "Sunday, 06-Nov-94 08:49:37 GMT"
constexpr auto s_ascTimeDateFormat = "ddd MMM d hh:mm:ss yyyy"_L1;          // e.g. "Sun Nov  6 08:49:37 1994"

constexpr auto s_cacheControlNoCache = "no-cache"_baview;
constexpr auto s_cacheControlMaxAge  = "max-age="_baview;

template <class Container,
          typename ValueType = typename Container::value_type,
          class Iterator = typename Container::const_iterator>
Iterator findPrefix(const Container &container, const ValueType &prefix)
{
    return std::find_if(container.cbegin(), container.cend(),
                        [prefix](const ValueType &token) {
        return token.startsWith(prefix);
    });
}

template <class String, class StringView = typename detail::view_trait<String>::type>
StringView suffixView(const String &text, qsizetype offset)
{
    return {text.cbegin() + offset, static_cast<compat::lentype>(text.length() - offset)};
}

} // namespace

Message Message::parse(const QByteArray &data)
{
    auto buffer = QBuffer{};
    buffer.setData(data);

    if (!buffer.open(QBuffer::ReadOnly))
        return {};

    return parse(&buffer);
}

Message Message::parse(QIODevice *device)
{
    if (!device->canReadLine())
        return {};

    const auto &statusLine = device->readLine().trimmed().split(' ');

    if (statusLine.size() != 3)
        return {};

    auto message = Message {
        statusLine[0],
        statusLine[1],
        statusLine[2],
    };

    while (device->canReadLine()) {
        const auto &line = device->readLine();
        const auto &trimmedLine = line.trimmed();

        if (trimmedLine.isEmpty())
            break;

        if (line[0] == ' ') {
            if (message.headers.isEmpty()) {
                qCWarning(lcHttpParser, "Ignoring invalid header line: %s", line.constData());
                continue;
            }

            message.headers.last().second.append(trimmedLine);
        } else if (const auto colon = line.indexOf(':'); colon > 0) {
            auto name  = line.left(colon).trimmed();
            auto value = line.mid(colon + 1).trimmed();

#if QT_VERSION_MAJOR >= 6
            message.headers.emplaceBack(std::move(name), std::move(value));
#else // QT_VERSION_MAJOR < 6
            message.headers.append({std::move(name), std::move(value)});
#endif // QT_VERSION_MAJOR < 6
        } else {
            qCWarning(lcHttpParser, "Ignoring invalid header line: %s", line.constData());
            continue;
        }
    }

    return message;
}

QDateTime parseDateTime(const QByteArray &text)
{
    return parseDateTime(QString::fromUtf8(text));
}

QDateTime parseDateTime(const QString &text)
{
    const auto locale = QLocale::c(); // for language neutral (that is English) weekdays

    if (auto dt = locale.toDateTime(text, s_rfc1123DateFormat); dt.isValid()) {
        dt.setTimeSpec(Qt::UTC);
        return dt;
    } else if (auto dt = locale.toDateTime(text, s_rfc850DateFormat); dt.isValid()) {
        dt.setTimeSpec(Qt::UTC);
        return dt;
    } else if (auto dt = locale.toDateTime(QString{text}.replace("  "_L1, " "_L1),
                                           s_ascTimeDateFormat); dt.isValid()) {
        dt.setTimeSpec(Qt::UTC);
        return dt;
    }

    return {};
}

QDateTime expiryDateTime(const QByteArray &cacheControl, const QByteArray &expires, const QDateTime &now)
{
    auto cacheControlWithoutSpaces = QByteArray{cacheControl}.replace(' ', compat::ByteArrayView{});
    const auto &cacheControlList = cacheControlWithoutSpaces.split(',');

    if (cacheControlList.contains(CaseInsensitive{s_cacheControlNoCache}))
        return now;

    if (const auto maxAge = findPrefix(cacheControlList, CaseInsensitive{s_cacheControlMaxAge});
            maxAge != cacheControlList.cend()) {
        const auto &value = suffixView(*maxAge, s_cacheControlMaxAge.length());

        if (const auto seconds = qnc::parse<uint>(value))
            return now.addSecs(*seconds);
    }

    if (!expires.isEmpty())
        return parseDateTime(expires);

    return {};
}

QDateTime expiryDateTime(const QByteArray &cacheControl, const QByteArray &expires)
{
    return expiryDateTime(cacheControl, expires, QDateTime::currentDateTimeUtc());
}

} // namespace qnc::http
