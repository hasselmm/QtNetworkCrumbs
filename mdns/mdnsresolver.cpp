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

using namespace std::chrono_literals;

Q_LOGGING_CATEGORY(lcResolver, "mdns.resolver")

constexpr auto s_mdnsUnicastIPv4 = "224.0.0.251"_L1;
constexpr auto s_mdnsUnicastIPv6 = "ff02::fb"_L1;
constexpr auto s_mdnsPort = 5353;

auto multicastGroup(QUdpSocket *socket)
{
    switch (socket->localAddress().protocol()) {
    case QUdpSocket::IPv4Protocol:
        return QHostAddress{s_mdnsUnicastIPv4};
    case QUdpSocket::IPv6Protocol:
        return QHostAddress{s_mdnsUnicastIPv6};

    case QUdpSocket::AnyIPProtocol:
    case QUdpSocket::UnknownNetworkLayerProtocol:
        break;
    }

    Q_UNREACHABLE();
    return QHostAddress{};
};

bool isSupportedInterfaceType(QNetworkInterface::InterfaceType type)
{
    return type == QNetworkInterface::Ethernet
            || type == QNetworkInterface::Wifi;
}

bool isMulticastInterface(const QNetworkInterface &iface)
{
    return iface.flags().testFlag(QNetworkInterface::IsRunning)
            && iface.flags().testFlag(QNetworkInterface::CanMulticast);
}

bool isSupportedInterface(const QNetworkInterface &iface)
{
    return isSupportedInterfaceType(iface.type())
            && isMulticastInterface(iface);
}

bool isLinkLocalAddress(const QHostAddress &address)
{
    return address.protocol() == QAbstractSocket::IPv4Protocol
            || address.isLinkLocal();
}

auto isSocketForAddress(QHostAddress address)
{
    return [address = std::move(address)](QUdpSocket *socket) {
        return socket->localAddress() == address;
    };
}

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
    , m_target{normalizedHostName(service.target().toByteArray(), domain)}
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
    : QObject{parent}
    , m_timer{new QTimer{this}}
    , m_domain{"local"_L1}
{
    m_timer->callOnTimeout(this, &Resolver::onTimeout);
    QTimer::singleShot(0, this, &Resolver::onTimeout);
    m_timer->start(2s);
}

void Resolver::setDomain(QString domain)
{
    if (std::exchange(m_domain, domain) != domain)
        emit domainChanged(m_domain);
}

QString Resolver::domain() const
{
    return m_domain;
}

void Resolver::setInterval(std::chrono::milliseconds ms)
{
    if (intervalAsDuration() != ms) {
        m_timer->setInterval(ms);
        emit intervalChanged(interval());
    }
}

void Resolver::setInterval(int ms)
{
    if (interval() != ms) {
        m_timer->setInterval(ms);
        emit intervalChanged(interval());
    }
}

std::chrono::milliseconds Resolver::intervalAsDuration() const
{
    return m_timer->intervalAsDuration();
}

int Resolver::interval() const
{
    return m_timer->interval();
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

bool Resolver::lookupServices(QStringList serviceTypes)
{
    auto message = qnc::mdns::Message{};

    for (const auto &type: serviceTypes)
        message.addQuestion({qualifiedHostName(type, m_domain).toLatin1(), qnc::mdns::Message::PTR});

    return lookup(message);
}

bool Resolver::lookup(Message query)
{
    if (const auto data = query.data(); !m_queries.contains(data)) {
        m_queries.append(std::move(data));
        return true;
    }

    return false;
}

bool Resolver::isOwnMessage(QNetworkDatagram message) const
{
    if (message.senderPort() != s_mdnsPort)
        return false;
    if (socketForAddress(message.senderAddress()))
        return false;

    return m_queries.contains(message.data());
}

QUdpSocket *Resolver::socketForAddress(QHostAddress address) const
{
    const auto it = std::find_if(m_sockets.begin(), m_sockets.end(), isSocketForAddress(std::move(address)));
    if (it != m_sockets.end())
        return *it;

    return nullptr;
}

void Resolver::scanNetworkInterfaces()
{
    auto newSocketList = QList<QUdpSocket *>{};

    const auto allInterfaces = QNetworkInterface::allInterfaces();
    for (const auto &iface: allInterfaces) {
        if (!isSupportedInterface(iface))
            continue;

        const auto addressEntries = iface.addressEntries();
        for (const auto &entry: addressEntries) {
            if (!isLinkLocalAddress(entry.ip()))
                continue;

            if (const auto socket = socketForAddress(entry.ip())) {
                newSocketList.append(socket);
                continue;
            }

            qCInfo(lcResolver) << "Creating socket for" << entry.ip();
            auto socket = std::make_unique<QUdpSocket>(this);

            if (!socket->bind(entry.ip(), s_mdnsPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
                qCWarning(lcResolver, "Could not bind mDNS socket to %ls: %ls",
                          qUtf16Printable(entry.ip().toString()),
                          qUtf16Printable(socket->errorString()));
                continue;
            }

            const auto group = multicastGroup(socket.get());
            if (socket->joinMulticastGroup(group, iface)) {
                qCDebug(lcResolver, "Multicast group %ls joined on %ls",
                        qUtf16Printable(group.toString()),
                        qUtf16Printable(iface.name()));
            } else if (socket->error() != QUdpSocket::UnknownSocketError) {
                qCWarning(lcResolver, "Could not join multicast group %ls on %ls: %ls",
                          qUtf16Printable(group.toString()),
                          qUtf16Printable(iface.name()),
                          qUtf16Printable(socket->errorString()));
                continue;
            }

            connect(socket.get(), &QUdpSocket::readyRead,
                    this, [this, socket = socket.get()] {
                onReadyRead(socket);
            });

            socket->setMulticastInterface(iface);
            newSocketList.append(socket.release());
        }
    }

    const auto oldSocketList = std::exchange(m_sockets, newSocketList);
    for (const auto socket: oldSocketList) {
        if (!m_sockets.contains(socket)) {
            qCDebug(lcResolver, "Destroying socket for address %ls",
                    qUtf16Printable(socket->localAddress().toString()));
            delete socket;
        }
    }
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
            const auto message = qnc::mdns::Message{received.data()};

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
                emit serviceResolved({m_domain, name, service, std::move(info)});
            }

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

} // namespace qnc::mdns

QDebug operator<<(QDebug debug, const qnc::mdns::ServiceDescription &service)
{
    const auto saver = QDebugStateSaver{debug};

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
