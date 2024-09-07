/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2023 Mathias Hasselmann
 */
#include "qncresolver.h"

// Qt headers
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QTimer>
#include <QUdpSocket>
#include <QVariant>

namespace qnc::core {

namespace {

using namespace std::chrono_literals;

Q_LOGGING_CATEGORY(lcResolver,  "qnc.core.resolver")
Q_LOGGING_CATEGORY(lcMulticast, "qnc.core.resolver.multicast")

auto isSocketForAddress(const QHostAddress &address)
{
    return [address = std::move(address)](QAbstractSocket *socket) {
        return socket->localAddress() == address
            || socket->peerAddress() == address;
    };
}

} // namespace

GenericResolver::GenericResolver(QObject *parent)
    : QObject{parent}
    , m_timer{new QTimer{this}}
{
    m_timer->callOnTimeout(this, &GenericResolver::onTimeout);
    QTimer::singleShot(0, this, &GenericResolver::onTimeout);
    m_timer->start(2s);
}

void GenericResolver::setScanInterval(std::chrono::milliseconds ms)
{
    if (scanIntervalAsDuration() != ms) {
        m_timer->setInterval(ms);
        emit scanIntervalChanged(scanInterval());
    }
}

void GenericResolver::setScanInterval(int ms)
{
    if (scanInterval() != ms) {
        m_timer->setInterval(ms);
        emit scanIntervalChanged(scanInterval());
    }
}

std::chrono::milliseconds GenericResolver::scanIntervalAsDuration() const
{
    return m_timer->intervalAsDuration();
}

int GenericResolver::scanInterval() const
{
    return m_timer->interval();
}

QAbstractSocket *GenericResolver::socketForAddress(const QHostAddress &address) const
{
    const auto it = std::find_if(m_sockets.begin(), m_sockets.end(),
                                 isSocketForAddress(address));
    if (it != m_sockets.end())
        return *it;

    return nullptr;
}

void GenericResolver::scanNetworkInterfaces()
{
    auto newSocketList = QList<QAbstractSocket *>{};

    const auto &allInterfaces = QNetworkInterface::allInterfaces();

    for (const auto &iface : allInterfaces) {
        if (!isSupportedInterface(iface))
            continue;

        const auto addressEntries = iface.addressEntries();
        for (const auto &entry : addressEntries) {
            if (!isSupportedAddress(entry.ip()))
                continue;

            if (const auto socket = socketForAddress(entry.ip())) {
                newSocketList.append(socket);
                continue;
            }

            qCInfo(lcResolver, "Creating socket for %ls on %ls",
                   qUtf16Printable(entry.ip().toString()),
                   qUtf16Printable(iface.humanReadableName()));

            if (const auto socket = createSocket(iface, entry.ip()))
                newSocketList.append(socket);
        }
    }

    const auto oldSocketList = std::exchange(m_sockets, newSocketList);

    for (const auto socket : oldSocketList) {
        if (!m_sockets.contains(socket)) {
            qCDebug(lcResolver, "Destroying socket for %ls",
                    qUtf16Printable(socket->localAddress().toString()));

            delete socket;
        }
    }
}

void GenericResolver::onTimeout()
{
    scanNetworkInterfaces();
    submitQueries(m_sockets);
}

bool MulticastResolver::isSupportedInterface(const QNetworkInterface &iface) const
{
    return isSupportedInterfaceType(iface)
            && isMulticastInterface(iface);
}

bool MulticastResolver::isSupportedInterfaceType(const QNetworkInterface &iface)
{
    return iface.type() == QNetworkInterface::Ethernet
            || iface.type() == QNetworkInterface::Wifi;
}

bool MulticastResolver::isMulticastInterface(const QNetworkInterface &iface)
{
    return iface.flags().testFlag(QNetworkInterface::IsRunning)
            && iface.flags().testFlag(QNetworkInterface::CanMulticast);
}

bool MulticastResolver::isSupportedAddress(const QHostAddress &address) const
{
    return isLinkLocalAddress(address);
}

bool MulticastResolver::isLinkLocalAddress(const QHostAddress &address)
{
    return address.protocol() == QAbstractSocket::IPv4Protocol
            || address.isLinkLocal();
}

bool MulticastResolver::addQuery(QByteArray &&query)
{
    if (!m_queries.contains(query)) {
        m_queries.append(std::move(query));
        return true;
    }

    return false;
}

QAbstractSocket *MulticastResolver::createSocket(const QNetworkInterface &iface,
                                                 const QHostAddress &address)
{
    auto socket = std::make_unique<QUdpSocket>(this);

    if (!socket->bind(address, port(), QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qCWarning(lcMulticast, "Could not bind multicast socket to %ls: %ls",
                  qUtf16Printable(address.toString()),
                  qUtf16Printable(socket->errorString()));
        return nullptr;
    }

    const auto &group = multicastGroup(address);

    if (socket->joinMulticastGroup(group, iface)) {
        qCDebug(lcMulticast, "Multicast group %ls joined on %ls",
                qUtf16Printable(group.toString()),
                qUtf16Printable(iface.humanReadableName()));
    } else if (socket->error() != QUdpSocket::UnknownSocketError) {
        qCWarning(lcMulticast, "Could not join multicast group %ls on %ls: %ls",
                  qUtf16Printable(group.toString()),
                  qUtf16Printable(iface.name()),
                  qUtf16Printable(socket->errorString()));
        return nullptr;
    }

    connect(socket.get(), &QUdpSocket::readyRead,
            this, [this, socket = socket.get()] {
        onDatagramReceived(socket);
    });

    socket->setMulticastInterface(iface);
    socket->setSocketOption(QUdpSocket::MulticastTtlOption, 4);

    return socket.release();
}

void MulticastResolver::submitQueries(const QList<QAbstractSocket *> &socketList)
{
    for (const auto abstractSocket: socketList) {
        Q_ASSERT(dynamic_cast<QUdpSocket *>(abstractSocket));

        const auto socket = static_cast<QUdpSocket *>(abstractSocket);
        const auto group = multicastGroup(socket->localAddress());

        for (const auto &data: m_queries)
            socket->writeDatagram(data, group, port());
    }
}

void MulticastResolver::onDatagramReceived(QUdpSocket *socket)
{
    while (socket->hasPendingDatagrams()) {
        const auto datagram = socket->receiveDatagram();

        if (!isOwnMessage(datagram))
            processDatagram(datagram);
    }
}

bool MulticastResolver::isOwnMessage(const QNetworkDatagram &message) const
{
    if (message.senderPort() != port())
        return false;
    if (socketForAddress(message.senderAddress()) == nullptr)
        return false;

    return m_queries.contains(message.data());
}

} // namespace qnc
