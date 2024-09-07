/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#ifndef QNC_QNCRESOLVER_H
#define QNC_QNCRESOLVER_H

#include <QObject>

class QAbstractSocket;
class QHostAddress;
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
    [[nodiscard]] virtual bool isSupportedInterface(const QNetworkInterface &iface) const = 0;
    [[nodiscard]] virtual bool isSupportedAddress(const QHostAddress &address) const = 0;

    [[nodiscard]] virtual QAbstractSocket *createSocket(const QNetworkInterface &iface,
                                                        const QHostAddress &address) = 0;

    [[nodiscard]] QAbstractSocket *socketForAddress(const QHostAddress &address) const;

    virtual void submitQueries(const QList<QAbstractSocket *> &socketList) = 0;

private:
    void onTimeout();
    void scanNetworkInterfaces();

    QTimer *const               m_timer;
    QList<QAbstractSocket *>    m_sockets;
};

class MulticastResolver : public GenericResolver
{
    Q_OBJECT

public:
    using GenericResolver::GenericResolver;

protected:
    [[nodiscard]] bool isSupportedInterface(const QNetworkInterface &iface) const override;
    [[nodiscard]] bool isSupportedAddress(const QHostAddress &address) const override;

    [[nodiscard]] QAbstractSocket *createSocket(const QNetworkInterface &iface,
                                                const QHostAddress &address) override;

    void submitQueries(const QList<QAbstractSocket *> &socketList) override;

    [[nodiscard]] virtual quint16 port() const = 0;
    [[nodiscard]] virtual QHostAddress multicastGroup(const QHostAddress &address) const = 0;
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
