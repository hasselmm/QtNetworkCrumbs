/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#include "httpparser.h"

// QtNetworkCrumbs headers
#include "literals.h"
#include "parse.h"

// Qt headers
#include <QBuffer>
#include <QDateTime>
#include <QLoggingCategory>
#include <QTimeZone>

namespace qnc::http {

namespace {

Q_LOGGING_CATEGORY(lcHttpParser, "qnc.http.parser")

// examples from https://www.rfc-editor.org/rfc/rfc9110#section-5.6.7
constexpr auto s_rfc1123DateFormat = "ddd, dd MMM yyyy hh:mm:ss 'GMT'"_L1;  // e.g. "Sun, 06 Nov 1994 08:49:37 GMT"
constexpr auto s_rfc850DateFormat = "dddd, dd-MMM-yy hh:mm:ss 'GMT'"_L1;    // e.g. "Sunday, 06-Nov-94 08:49:37 GMT"
constexpr auto s_ascTimeDateFormat = "ddd MMM d hh:mm:ss yyyy"_L1;          // e.g. "Sun Nov  6 08:49:37 1994"

constexpr auto s_cacheControlNoCache = "no-cache"_baview;
constexpr auto s_cacheControlMaxAge  = "max-age="_baview;

constexpr auto s_protocolPrefixHttp = "HTTP/"_baview;

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

Message Message::parseStatusLine(const QByteArray &line)
{
    auto message = Message{};

    if (const auto &status = line.trimmed().split(' ');
            Q_LIKELY(status.size() == 3)) {
        if (status.first().startsWith(s_protocolPrefixHttp)) {
            message.m_type   = Type::Response;
            message.m_status = status;
        } else if (status.last().startsWith(s_protocolPrefixHttp)) {
            message.m_type   = Type::Request;
            message.m_status = status;
        }
    }

    return message;
}

QByteArray Message::statusField(Type expectedType, int index) const
{
    if (Q_UNLIKELY(type() != expectedType))
        return {};

    return m_status[index];
}

QByteArray Message::protocol() const
{
    switch (type()) {
    case Type::Request:
        return m_status.last();
    case Type::Response:
        return m_status.first();
    case Type::Invalid:
        break;
    }

    return {};
}

QByteArray Message::verb() const
{
    return statusField(Type::Request, 0);
}

QByteArray Message::resource() const
{
    return statusField(Type::Request, 1);
}

std::optional<uint> Message::statusCode() const
{
    return core::parse<uint>(statusField(Type::Response, 1));
}

QByteArray Message::statusPhrase() const
{
    return statusField(Type::Response, 2);
}

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

    auto message = Message::parseStatusLine(device->readLine());

    if (message.isInvalid())
        return {};

    while (device->canReadLine()) {
        const auto &line = device->readLine();
        const auto &trimmedLine = line.trimmed();

        if (trimmedLine.isEmpty())
            break;

        if (line[0] == ' ') {
            if (message.m_headers.isEmpty()) {
                qCWarning(lcHttpParser, "Ignoring invalid header line: %s", line.constData());
                continue;
            }

            message.m_headers.last().second.append(trimmedLine);
        } else if (const auto colon = line.indexOf(':'); colon > 0) {
            auto name  = line.left(colon).trimmed();
            auto value = line.mid(colon + 1).trimmed();

#if QT_VERSION_MAJOR >= 6
            message.m_headers.emplaceBack(std::move(name), std::move(value));
#else // QT_VERSION_MAJOR < 6
            message.m_headers.append({std::move(name), std::move(value)});
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
        dt.setTimeZone(QTimeZone::utc());
        return dt;
    } else if (auto dt = locale.toDateTime(text, s_rfc850DateFormat); dt.isValid()) {
        dt.setTimeZone(QTimeZone::utc());
        return dt;
    } else if (auto dt = locale.toDateTime(QString{text}.replace("  "_L1, " "_L1),
                                           s_ascTimeDateFormat); dt.isValid()) {
        dt.setTimeZone(QTimeZone::utc());
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

        if (const auto seconds = core::parse<uint>(value))
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
