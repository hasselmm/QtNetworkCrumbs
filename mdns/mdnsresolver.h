/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2021 Mathias Hasselmann
 */
#ifndef MDNS_MDNSRESOLVER_H
#define MDNS_MDNSRESOLVER_H

#include <QObject>

class QHostAddress;
class QNetworkDatagram;
class QTimer;
class QUdpSocket;

namespace MDNS {

class Message;
class ServiceRecord;

class ServiceDescription
{
    Q_GADGET

public:
    ServiceDescription(QString domain, QByteArray name, ServiceRecord service, QByteArray info);

    auto name() const { return m_name; };
    auto type() const { return m_type; }
    auto target() const { return m_target; }
    auto priority() const { return m_priority; }
    auto weight() const { return m_weight; }
    auto port() const { return m_port; }
    auto info() const { return m_info; }

private:
    QString m_name;
    QString m_type;
    QString m_target;

    int m_port;
    int m_priority;
    int m_weight;

    QByteArray m_info;
};

class Resolver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString domain READ domain WRITE setDomain NOTIFY domainChanged FINAL)
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged FINAL)

public:
    explicit Resolver(QObject *parent = {});

    QString domain() const;

    int interval() const;
    std::chrono::milliseconds intervalAsDuration() const;
    void setInterval(std::chrono::milliseconds ms);

public slots:
    void setDomain(QString domain);
    void setInterval(int ms);

    bool lookupHostNames(QStringList hostNames);
    bool lookupServices(QStringList serviceTypes);
    bool lookup(MDNS::Message query);

signals:
    void domainChanged(QString domain);
    void intervalChanged(int interval);

    void hostNameResolved(QString hostname, QList<QHostAddress> addresses);
    void serviceResolved(MDNS::ServiceDescription service);
    void messageReceived(MDNS::Message message);

private:
    bool isOwnMessage(QNetworkDatagram message) const;
    QUdpSocket *socketForAddress(QHostAddress address) const;

    void scanNetworkInterfaces();
    void submitQueries() const;

    void onReadyRead(QUdpSocket *socket);
    void onTimeout();

    QList<QUdpSocket *> m_sockets;
    QTimer *const m_timer;

    QString m_domain;
    QByteArrayList m_queries;
};

} // namespace MDNS

QDebug operator<<(QDebug debug, const MDNS::ServiceDescription &service);

#endif // MDNS_MDNSRESOLVER_H
