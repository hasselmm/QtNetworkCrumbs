/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#include "mdnsresolver.h"

// QtNetworkCrumbs headers
#include "mdnsmessage.h"
#include "qncliterals.h"

// Qt headers
#include <QHostAddress>
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QTimer>
#include <QUdpSocket>

// STL headers
#include <unordered_map>

namespace qnc::mdns {

namespace {

Q_LOGGING_CATEGORY(lcResolver, "qnc.mdns.resolver")

constexpr auto s_mdnsUnicastIPv4 = "224.0.0.251"_L1;
constexpr auto s_mdnsUnicastIPv6 = "ff02::fb"_L1;
constexpr auto s_mdnsPort = 5353;

auto normalizedHostName(QByteArray name, QString domain)
{
    auto normalizedName = QString::fromLatin1(name);

    if (normalizedName.endsWith('.'_L1))
        normalizedName.truncate(normalizedName.length() - 1);
    if (normalizedName.endsWith('.'_L1 + domain))
        normalizedName.truncate(normalizedName.length() - domain.length() - 1);

    return normalizedName;
}

auto qualifiedHostName(QString name, QString domain)
{
    if (name.endsWith('.'_L1))
        name.truncate(name.length() - 1);
    else if (const auto domainSuffix = '.'_L1 + domain; !name.endsWith(domainSuffix))
        name.append(domainSuffix);

    return name;
}

auto parseTxtRecord(const QByteArray &txtRecord)
{
    auto stringList = QStringList{};

    const auto first = txtRecord.cbegin();
    const auto last = txtRecord.cend();

    for (auto it = first; it < last; ) {
        const auto length = static_cast<int>(static_cast<quint8>(*it));

        if (it + length >= last) {
            const auto offset = static_cast<int>(it - first);
            qCWarning(lcResolver, "Malformed TXT record at offset %d", offset);
            break;
        }

        it += 1;

        stringList += QString::fromUtf8(it, length);
        it += length;
    }

    return stringList;
}

} // namespace

ServiceDescription::ServiceDescription(QString domain, QByteArray name, ServiceRecord service, QStringList info)
    : m_name{normalizedHostName(name, domain)}
    , m_target{qualifiedHostName(service.target().toString(), domain)}
    , m_port{service.port()}
    , m_priority{service.priority()}
    , m_weight{service.weight()}
    , m_info{std::move(info)}
{
    if (const auto separator = m_name.indexOf('.'_L1); separator >= 0) {
        m_type = m_name.mid(separator + 1);
        m_name.truncate(separator);
    }
}

Resolver::Resolver(QObject *parent)
    : core::MulticastResolver{parent}
    , m_domain{"local"_L1}
{}

void Resolver::setDomain(QString domain)
{
    if (std::exchange(m_domain, domain) != domain)
        emit domainChanged(m_domain);
}

QString Resolver::domain() const
{
    return m_domain;
}

QHostAddress Resolver::multicastGroup(const QHostAddress &address) const
{
    switch (address.protocol()) {
    case QUdpSocket::IPv4Protocol:
        return QHostAddress{s_mdnsUnicastIPv4};
    case QUdpSocket::IPv6Protocol:
        return QHostAddress{s_mdnsUnicastIPv6};

    case QUdpSocket::AnyIPProtocol:
    case QUdpSocket::UnknownNetworkLayerProtocol:
        break;
    }

    Q_UNREACHABLE();
    return {};
}

bool Resolver::lookupServices(QStringList serviceTypes)
{
    auto message = qnc::mdns::Message{};

    for (const auto &type: serviceTypes)
        message.addQuestion({qualifiedHostName(type, m_domain).toLatin1(), qnc::mdns::Message::PTR});

    return lookup(message);
}

bool Resolver::lookup(Message query)
{
    return addQuery(query.data());
}

quint16 Resolver::port() const
{
    return s_mdnsPort;
}

bool Resolver::lookupHostNames(QStringList hostNames)
{
    auto message = qnc::mdns::Message{};

    for (const auto &name: hostNames) {
        message.addQuestion({qualifiedHostName(name, m_domain).toLatin1(), qnc::mdns::Message::A});
        message.addQuestion({qualifiedHostName(name, m_domain).toLatin1(), qnc::mdns::Message::AAAA});
    }

    return lookup(message);
}

void Resolver::processDatagram(const QNetworkDatagram &datagram)
{
    const auto message = qnc::mdns::Message{datagram.data()};

    auto resolvedAddresses = std::unordered_map<QByteArray, QList<QHostAddress>>{};
    auto resolvedServices = std::unordered_map<QByteArray, ServiceRecord>{};
    auto resolvedText = std::unordered_map<QByteArray, QByteArray>{};

    for (auto i = 0; i < message.responseCount(); ++i) {
        const auto response = message.response(i);

        if (const auto address = response.address(); !address.isNull()) {
            auto &knownAddresses = resolvedAddresses[response.name().toByteArray()];
            if (!knownAddresses.contains(address))
                knownAddresses.append(address);
        } else if (const auto service = response.service(); !service.isNull()) {
            resolvedServices.insert({response.name().toByteArray(), service});
        } else if (const auto text = response.text(); !text.isNull()) {
            resolvedText.insert({response.name().toByteArray(), text});
        }
    }

    for (const auto &[name, service]: resolvedServices) {
        auto info = parseTxtRecord(resolvedText[name]);
        emit serviceFound({m_domain, name, service, std::move(info)});
    }

    for (const auto &[name, addresses]: resolvedAddresses)
        emit hostNameFound(normalizedHostName(name, m_domain), addresses);

    emit messageReceived(std::move(message));
}

} // namespace qnc::mdns

QDebug operator<<(QDebug debug, const qnc::mdns::ServiceDescription &service)
{
    const auto _ = QDebugStateSaver{debug};

    if (debug.verbosity() >= QDebug::DefaultVerbosity)
        debug.nospace() << service.staticMetaObject.className();

    return debug.nospace()
            << "(" << service.name()
            << ", type=" << service.type()
            << ", target=" << service.target()
            << ", port=" << service.port()
            << ", priority=" << service.priority()
            << ", weight=" << service.weight()
            << ", info=" << service.info()
            << ")";
}

#include "moc_mdnsresolver.cpp"
