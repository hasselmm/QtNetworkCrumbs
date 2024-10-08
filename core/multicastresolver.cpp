/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#include "multicastresolver.h"

// Qt headers
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QUdpSocket>
#include <QVariant>

namespace qnc::core {

namespace {

Q_LOGGING_CATEGORY(lcMulticast, "qnc.core.resolver.multicast")

QHostAddress wildcardAddress(const QHostAddress &address)
{
    switch(address.protocol()) {
    case QUdpSocket::IPv4Protocol:
        return QHostAddress::AnyIPv4;

    case QUdpSocket::IPv6Protocol:
        return QHostAddress::AnyIPv6;

    case QUdpSocket::AnyIPProtocol:
    case QUdpSocket::UnknownNetworkLayerProtocol:
        break;
    }

    Q_UNREACHABLE();
    return {};
}

} // namespace

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

MulticastResolver::SocketPointer
MulticastResolver::createSocket(const QNetworkInterface &iface, const QHostAddress &address)
{
    auto socket = std::make_shared<QUdpSocket>(this);

    const auto &bindAddress = wildcardAddress(address);
    const auto &bindMode = QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint;
    constexpr auto randomPort = 0;

    if (!socket->bind(bindAddress, randomPort, bindMode)) {
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

    return socket;
}

void MulticastResolver::submitQueries(const SocketTable &sockets)
{
    for (auto it = sockets.cbegin(); it != sockets.cend(); ++it) {
        Q_ASSERT(dynamic_cast<QUdpSocket *>(it->get()));

        const auto &address = it.key();
        const auto socket = static_cast<QUdpSocket *>(it->get());
        const auto group = multicastGroup(address);

        for (const auto &data: m_queries) {
            const auto query = finalizeQuery(address, data);
            socket->writeDatagram(query, group, port());
        }
    }
}

QByteArray MulticastResolver::finalizeQuery(const QHostAddress &/*address*/, const QByteArray &query) const
{
    return query;
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
