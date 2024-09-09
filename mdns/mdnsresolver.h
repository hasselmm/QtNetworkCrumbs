/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#ifndef MDNS_MDNSRESOLVER_H
#define MDNS_MDNSRESOLVER_H

#include <QObject>

class QHostAddress;
class QNetworkDatagram;
class QTimer;
class QUdpSocket;

namespace qnc::mdns {

class Message;
class ServiceRecord;

class ServiceDescription
{
    Q_GADGET

public:
    ServiceDescription(QString domain, QByteArray name, ServiceRecord service, QStringList info);

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

    QStringList m_info;
};

class Resolver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString domain READ domain WRITE setDomain NOTIFY domainChanged FINAL)
    Q_PROPERTY(int scanInterval READ scanInterval WRITE setScanInterval NOTIFY scanIntervalChanged FINAL)

public:
    explicit Resolver(QObject *parent = {});

    QString domain() const;

    int scanInterval() const;
    std::chrono::milliseconds scanIntervalAsDuration() const;
    void setScanInterval(std::chrono::milliseconds ms);

public slots:
    void setDomain(QString domain);
    void setScanInterval(int ms);

    bool lookupHostNames(QStringList hostNames);
    bool lookupServices(QStringList serviceTypes);
    bool lookup(qnc::mdns::Message query);

signals:
    void domainChanged(QString domain);
    void scanIntervalChanged(int interval);

    void hostNameResolved(QString hostname, QList<QHostAddress> addresses);
    void serviceResolved(qnc::mdns::ServiceDescription service);
    void messageReceived(qnc::mdns::Message message);

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

} // namespace qnc::mdns

QDebug operator<<(QDebug debug, const qnc::mdns::ServiceDescription &service);

#endif // MDNS_MDNSRESOLVER_H
