/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#ifndef QNCCORE_UNICASTRESOLVER_H
#define QNCCORE_UNICASTRESOLVER_H

#include "abstractresolver.h"

class QNetworkDatagram;
class QUdpSocket;

namespace qnc::core {

class MulticastResolver : public AbstractResolver
{
    Q_OBJECT

public:
    using AbstractResolver::AbstractResolver;

protected:
    [[nodiscard]] bool isSupportedInterface(const QNetworkInterface &iface) const override;
    [[nodiscard]] bool isSupportedAddress(const QHostAddress &address) const override;

    [[nodiscard]] SocketPointer createSocket(const QNetworkInterface &iface,
                                             const QHostAddress &address) override;

    void submitQueries(const SocketTable &sockets) override;

    [[nodiscard]] virtual quint16 port() const = 0;
    [[nodiscard]] virtual QHostAddress multicastGroup(const QHostAddress &address) const = 0;
    [[nodiscard]] virtual QByteArray finalizeQuery(const QHostAddress &address, const QByteArray &query) const;

    virtual void processDatagram(const QNetworkDatagram &message) = 0;

    [[nodiscard]] static bool isSupportedInterfaceType(const QNetworkInterface &iface);
    [[nodiscard]] static bool isMulticastInterface(const QNetworkInterface &iface);
    [[nodiscard]] static bool isLinkLocalAddress(const QHostAddress &address);

    bool addQuery(QByteArray &&query);

private:
    void onDatagramReceived(QUdpSocket *socket);
    bool isOwnMessage(const QNetworkDatagram &message) const;

    QByteArrayList m_queries;
};

} // namespace qnc::core

#endif // QNCCORE_UNICASTRESOLVER_H
