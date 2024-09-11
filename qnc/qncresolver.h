/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#ifndef QNC_QNCRESOLVER_H
#define QNC_QNCRESOLVER_H

#include <QAbstractSocket>
#include <QHash>
#include <QHostAddress>
#include <QObject>

class QNetworkDatagram;
class QNetworkInterface;
class QTimer;
class QUdpSocket;

namespace qnc::core {

class GenericResolver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int scanInterval READ scanInterval WRITE setScanInterval NOTIFY scanIntervalChanged FINAL)

public:
    GenericResolver(QObject *parent = nullptr);

    [[nodiscard]] int scanInterval() const;
    [[nodiscard]] std::chrono::milliseconds scanIntervalAsDuration() const;
    void setScanInterval(std::chrono::milliseconds ms);

public slots:
    void setScanInterval(int ms);

signals:
    void scanIntervalChanged(int interval);

protected:
    using SocketPointer = std::shared_ptr<QAbstractSocket>;
    using SocketTable   = QHash<QHostAddress, SocketPointer>;

    [[nodiscard]] virtual bool isSupportedInterface(const QNetworkInterface &iface) const = 0;
    [[nodiscard]] virtual bool isSupportedAddress(const QHostAddress &address) const = 0;

    [[nodiscard]] virtual SocketPointer createSocket(const QNetworkInterface &iface,
                                                     const QHostAddress &address) = 0;

    [[nodiscard]] SocketPointer socketForAddress(const QHostAddress &address) const;

    virtual void submitQueries(const SocketTable &sockets) = 0;

private:
    void onTimeout();
    void scanNetworkInterfaces();

    QTimer *const   m_timer;
    SocketTable     m_sockets;
};

class MulticastResolver : public GenericResolver
{
    Q_OBJECT

public:
    using GenericResolver::GenericResolver;

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

#endif // QNC_QNCRESOLVER_H
