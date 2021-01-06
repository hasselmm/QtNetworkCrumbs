/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2021 Mathias Hasselmann
 */
#include "mdnsresolver.h"

// MDNS headers
#include "mdnsmessage.h"

// Qt headers
#include <QHostAddress>
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QTimer>
#include <QUdpSocket>

// STL headers
#include <unordered_map>

namespace MDNS {

namespace {

using namespace std::chrono_literals;

Q_LOGGING_CATEGORY(lcResolver, "mdns.resolver")

constexpr auto s_mdnsPort = 5353;

auto multicastGroup(QUdpSocket *socket)
{
    switch (socket->localAddress().protocol()) {
    case QUdpSocket::IPv4Protocol:
        return QHostAddress{"224.0.0.251"};
    case QUdpSocket::IPv6Protocol:
        return QHostAddress{"ff02::fb"};
    default:
        Q_UNREACHABLE();
        return QHostAddress{};
    }
};

auto normalizedHostName(QByteArray name, QString domain)
{
    auto normalizedName = QString::fromLatin1(name);

    if (normalizedName.endsWith('.'))
        normalizedName.truncate(normalizedName.length() - 1);
    if (normalizedName.endsWith('.' + domain))
        normalizedName.truncate(normalizedName.length() - domain.length() - 1);

    return normalizedName;
}

auto qualifiedHostName(QString name, QString domain)
{
    if (!name.endsWith('.'))
        return name + '.' + domain;

    return name;
}

}

ServiceDescription::ServiceDescription(QString domain, QByteArray name, ServiceRecord service, QByteArray info)
    : m_name{normalizedHostName(name, domain)}
    , m_target{normalizedHostName(service.target().toByteArray(), domain)}
    , m_port{service.port()}
    , m_priority{service.priority()}
    , m_weight{service.weight()}
    , m_info{std::move(info)}
{
    if (const auto separator = m_name.indexOf('.'); separator >= 0) {
        m_type = m_name.mid(separator + 1);
        m_name.truncate(separator);
    }
}

Resolver::Resolver(QObject *parent)
    : QObject{parent}
    , m_timer{new QTimer{this}}
    , m_domain{"local"}
{
    createSockets();
    connect(m_timer, &QTimer::timeout, this, &Resolver::onTimeout);
    QTimer::singleShot(0, this, &Resolver::onTimeout);
    m_timer->start(2s);
}

QString Resolver::domain() const
{
    return m_domain;
}

void Resolver::setDomain(QString domain)
{
    if (std::exchange(m_domain, domain) != domain)
        emit domainChanged(m_domain);
}

void Resolver::lookupHostNames(QStringList hostNames)
{
    MDNS::Message message;

    for (const auto &name: hostNames) {
        message.addQuestion({qualifiedHostName(name, m_domain).toLatin1(), MDNS::Message::A});
        message.addQuestion({qualifiedHostName(name, m_domain).toLatin1(), MDNS::Message::AAAA});
    }

    lookup(message);
}

void Resolver::lookupServices(QStringList serviceTypes)
{
    MDNS::Message message;

    for (const auto &type: serviceTypes)
        message.addQuestion({(type + m_domain).toLatin1(), MDNS::Message::PTR});

    lookup(message);
}

void Resolver::lookup(Message query)
{
    if (const auto data = query.data(); !m_queries.contains(data))
        m_queries.append(std::move(data));
}

bool Resolver::isOwnMessage(QNetworkDatagram message) const
{
    if (message.senderPort() != s_mdnsPort)
        return false;
    if (!m_ownAddresses.contains(message.senderAddress()))
        return false;

    return m_queries.contains(message.data());
}

void Resolver::createSockets()
{
    for (const QHostAddress &address: {QHostAddress::AnyIPv4, QHostAddress::AnyIPv6}) {
        auto socket = std::make_unique<QUdpSocket>(this);

        if (!socket->bind(address, s_mdnsPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            qCWarning(lcResolver, "Could not bind mDNS socket to %ls: %ls",
                      qUtf16Printable(address.toString()),
                      qUtf16Printable(socket->errorString()));
            continue;
        }

        connect(socket.get(), &QUdpSocket::readyRead,
                this, [this, socket = socket.get()] {
            onReadyRead(socket);
        });

        m_sockets.append(socket.release());
    }
}

void Resolver::scanNetworkInterfaces()
{
    const auto isSupportedType = [](const auto type) {
        return type == QNetworkInterface::Ethernet
                || type == QNetworkInterface::Wifi;
    };

    const auto hasMulticastFlags = [](const auto flags) {
        return flags.testFlag(QNetworkInterface::IsRunning)
                && flags.testFlag(QNetworkInterface::CanMulticast);
    };

    QList<QHostAddress> ownAddresses;

    const auto allInterfaces = QNetworkInterface::allInterfaces();
    for (const auto &iface: allInterfaces) {
        if (isSupportedType(iface.type())
                && hasMulticastFlags(iface.flags())) {
            const auto allAddresses = iface.allAddresses();
            for (const auto &address: allAddresses) {
                if (!ownAddresses.contains(address))
                    ownAddresses.append(address);
            }

            for (const auto socket: qAsConst(m_sockets)) {
                const auto group = multicastGroup(socket);
                if (socket->joinMulticastGroup(group, iface)) {
                    qCDebug(lcResolver, "Multicast group %ls joined",
                            qUtf16Printable(group.toString()));
                } else if (socket->error() != QUdpSocket::UnknownSocketError) {
                    qCWarning(lcResolver, "Could not join multicast group %ls: %ls",
                              qUtf16Printable(group.toString()),
                              qUtf16Printable(socket->errorString()));
                }
            }
        }
    }

    m_ownAddresses = std::move(ownAddresses);
}

void Resolver::submitQueries() const
{
    for (const auto socket: m_sockets) {
        const auto group = multicastGroup(socket);

        for (const auto &data: m_queries)
            socket->writeDatagram(data, group, 5353);
    }
}

void Resolver::onReadyRead(QUdpSocket *socket)
{
    while (socket->hasPendingDatagrams()) {
        if (const auto received = socket->receiveDatagram(); !isOwnMessage(received)) {
            const MDNS::Message message{received.data()};

            std::unordered_map<QByteArray, QList<QHostAddress>> resolvedAddresses;
            std::unordered_map<QByteArray, ServiceRecord> resolvedServices;
            std::unordered_map<QByteArray, QByteArray> resolvedText;

            for (int i = 0; i < message.responseCount(); ++i) {
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

            for (const auto &[name, service]: resolvedServices)
                emit serviceResolved({m_domain, name, service, resolvedText[name]});
            for (const auto &[name, addresses]: resolvedAddresses)
                emit hostNameResolved(normalizedHostName(name, m_domain), addresses);

            emit messageReceived(std::move(message));
        }
    }
}

void Resolver::onTimeout()
{
    scanNetworkInterfaces();
    submitQueries();
}

} // namespace MDNS

QDebug operator<<(QDebug debug, const MDNS::ServiceDescription &service)
{
    const QDebugStateSaver saver{debug};

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
