/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#include "ssdpresolver.h"

// QtNetworkCrumbs headers
#include "httpparser.h"
#include "qncliterals.h"

// Qt headers
#include <QHostAddress>
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QUrl>

namespace qnc::ssdp {

namespace {

Q_LOGGING_CATEGORY(lcResolver,   "qnc.ssdp.resolver")

constexpr auto s_ssdpUnicastIPv4 = "239.255.255.250"_L1;
constexpr auto s_ssdpUnicastIPv6 = "ff02::c"_L1;
constexpr auto s_ssdpPort = quint16{1900};

constexpr auto s_ssdpKeyMulticastGroup  = "{multicast-group}"_baview;
constexpr auto s_ssdpKeyUdpPort         = "{udp-port}"_baview;
constexpr auto s_ssdpKeyMinimumDelay    = "{minimum-delay}"_baview;
constexpr auto s_ssdpKeyMaximumDelay    = "{maximum-delay}"_baview;
constexpr auto s_ssdpKeyServiceType     = "{service-type}"_baview;

constexpr auto s_ssdpQueryTemplate = "M-SEARCH * HTTP/1.1\r\n"
                                     "ST: {service-type}\r\n"
                                     "MAN: \"ssdp:discover\"\r\n"
                                     "HOST: {multicast-group}:{udp-port}\r\n"
                                     "MX: {maximum-delay}\r\n"
                                     "MM: {minimum-delay}\r\n"
                                     "Content-Length: 0\r\n"
                                     "\r\n"_baview;

QList<QUrl> parseAlternativeLocations(compat::ByteArrayView text)
{
    auto locations = QList<QUrl>{};

    for (auto offset = qsizetype{0};;) {
        const auto start = text.indexOf('<', offset);

        if (start < 0)
            break;

        const auto end = text.indexOf('>', start + 1);

        if (end < 0)
            break;

        locations += QUrl::fromEncoded({text.data() + start + 1, end - start - 1});
        offset = end + 1;
    }

    return locations;
}

} // namespace

bool Resolver::lookupService(const ServiceLookupRequest &request)
{
    const auto minimumDelaySeconds = static_cast<int>(request.minimumDelay.count());
    const auto maximumDelaySeconds = static_cast<int>(request.maximumDelay.count());

    auto query =
            s_ssdpQueryTemplate.toByteArray().
            replace(s_ssdpKeyUdpPort,      QByteArray::number(s_ssdpPort)).
            replace(s_ssdpKeyMinimumDelay, QByteArray::number(minimumDelaySeconds)).
            replace(s_ssdpKeyMaximumDelay, QByteArray::number(maximumDelaySeconds)).
            replace(s_ssdpKeyServiceType,  request.serviceType.toUtf8());

    return addQuery(std::move(query));
}

bool Resolver::lookupService(const QString &serviceType)
{
    auto request = ServiceLookupRequest{};
    request.serviceType = serviceType;
    return lookupService(request);
}

QHostAddress Resolver::multicastGroup(const QHostAddress &address) const
{
    switch (address.protocol()) {
    case QAbstractSocket::IPv4Protocol:
        return QHostAddress{s_ssdpUnicastIPv4};

    case QAbstractSocket::IPv6Protocol:
        return QHostAddress{s_ssdpUnicastIPv6};

    case QAbstractSocket::AnyIPProtocol:
    case QAbstractSocket::UnknownNetworkLayerProtocol:
        break;
    }

    Q_UNREACHABLE();
    return {};
}

quint16 Resolver::port() const
{
    return s_ssdpPort;
}

QByteArray Resolver::finalizeQuery(const QHostAddress &address, const QByteArray &query) const
{
    const auto &group = multicastGroup(address);
    return QByteArray{query}.replace(s_ssdpKeyMulticastGroup, group.toString().toLatin1());
}

NotifyMessage NotifyMessage::parse(const QByteArray &data, const QDateTime &now)
{
    constexpr auto s_ssdpVerbSearch                 = "M-SEARCH"_baview;
    constexpr auto s_ssdpVerbNotify                 = "NOTIFY"_baview;
    constexpr auto s_ssdpResourceAny                = "*"_baview;
    constexpr auto s_ssdpProtocolHttp11             = "HTTP/1.1"_baview;
    constexpr auto s_ssdpHeaderCacheControl         = "Cache-Control"_baview;
    constexpr auto s_ssdpHeaderExpires              = "Expires"_baview;
    constexpr auto s_ssdpHeaderLocation             = "Location"_baview;
    constexpr auto s_ssdpHeaderAlternativeLocation  = "AL"_baview;
    constexpr auto s_ssdpHeaderNotifySubType        = "NTS"_baview;
    constexpr auto s_ssdpHeaderNotifyType           = "NT"_baview;
    constexpr auto s_ssdpHeaderUniqueServiceName    = "USN"_baview;
    constexpr auto s_ssdpNotifySubTypeAlive         = "ssdp:alive"_baview;
    constexpr auto s_ssdpNotifySubTypeByeBye        = "ssdp:byebye"_baview;

    const auto &message = http::Message::parse(data);

    if (message.isInvalid()) {
        qCWarning(lcResolver, "Ignoring malformed HTTP message");
        return {};
    }

    if (message.protocol() != s_ssdpProtocolHttp11) {
        qCWarning(lcResolver, "Ignoring unknown protocol: %s", message.protocol().constData());
        return {};
    }

    if (message.type() == http::Message::Type::Request) {
        if (message.verb() == s_ssdpVerbSearch)
            return {};

        if (message.verb() != s_ssdpVerbNotify) {
            qCDebug(lcResolver, "Ignoring unsupported verb: %s", message.verb().constData());
            return {};
        }

        if (message.resource() != s_ssdpResourceAny) {
            qCDebug(lcResolver, "Ignoring unsupported resource: %s", message.resource().constData());
            return {};
        }
    } else if (message.type() == http::Message::Type::Response) {
        if (message.statusCode() != 200) {
            qCDebug(lcResolver, "Ignoring unsupported status code: %d", message.statusCode().value());
            return {};
        }
    } else {
        qCWarning(lcResolver, "Ignoring unexpected HTTP message");
        return {};
    }

    auto response     = NotifyMessage{};
    auto notifyType   = QByteArray{};
    auto cacheControl = QByteArray{};
    auto expires      = QByteArray{};

    for (const auto &[name, value] : message.headers()) {
        if (name == s_ssdpHeaderUniqueServiceName)
            response.serviceName = QUrl::fromPercentEncoding(value);
        else if (name == s_ssdpHeaderNotifyType)
            response.serviceType = QUrl::fromPercentEncoding(value);
        else if (name == s_ssdpHeaderNotifySubType)
            notifyType = value;
        else if (name == s_ssdpHeaderCacheControl)
            cacheControl = value;
        else if (name == s_ssdpHeaderExpires)
            expires = value;
        else if (name == s_ssdpHeaderLocation)
            response.locations += QUrl::fromEncoded(value);
        else if (name == s_ssdpHeaderAlternativeLocation)
            response.altLocations += parseAlternativeLocations(value);
    }

    if (message.type() == http::Message::Type::Request) {
        if (notifyType == s_ssdpNotifySubTypeAlive)
            response.type = NotifyMessage::Type::Alive;
        else if (notifyType == s_ssdpNotifySubTypeByeBye)
            response.type = NotifyMessage::Type::ByeBye;
        else
            return {};
    } else if (message.type() == http::Message::Type::Response) {
        response.type = NotifyMessage::Type::Alive;
    }

    response.expiry = http::expiryDateTime(cacheControl, expires, now);

    return response;
}

NotifyMessage NotifyMessage::parse(const QByteArray &data)
{
    return parse(data, QDateTime::currentDateTimeUtc());
}

void Resolver::processDatagram(const QNetworkDatagram &datagram)
{
    const auto response = NotifyMessage::parse(datagram.data());

    switch (response.type) {
    case NotifyMessage::Type::Alive:
        emit serviceFound({response.serviceName, response.serviceType,
                           response.locations, response.altLocations,
                           response.expiry});
        break;

    case NotifyMessage::Type::ByeBye:
        emit serviceLost(response.serviceName);
        break;

    case NotifyMessage::Type::Invalid:
        break;
    }
}

// namespace

} // namespace qnc::ssdp

QDebug operator<<(QDebug debug, const qnc::ssdp::ServiceDescription &service)
{
    const auto _ = QDebugStateSaver{debug};

    if (debug.verbosity() >= QDebug::DefaultVerbosity)
        debug.nospace() << service.staticMetaObject.className();

    return debug.nospace()
            << "(" << service.name()
            << ", type=" << service.type()
            << ", location=" << service.locations()
            << ", alt-location=" << service.alternativeLocations()
            << ", expires=" << service.expires()
            << ")";
}
