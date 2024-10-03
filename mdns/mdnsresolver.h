/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#ifndef QNCMDNS_MDNSRESOLVER_H
#define QNCMDNS_MDNSRESOLVER_H

#include "multicastresolver.h"

class QHostAddress;
class QNetworkDatagram;
class QUdpSocket;

namespace qnc::mdns {

class Message;
class ServiceRecord;

class ServiceDescription
{
    Q_GADGET

public:
    ServiceDescription() = default;
    ServiceDescription(QString domain, QByteArray name, ServiceRecord service, QStringList info);

    auto name() const { return m_name; };
    auto type() const { return m_type; }
    auto target() const { return m_target; }
    auto priority() const { return m_priority; }
    auto weight() const { return m_weight; }
    auto port() const { return m_port; }
    auto info() const { return m_info; }

    QString info(const QString &key) const;
    QList<QUrl> locations() const;

private:
    QString m_name;
    QString m_type;
    QString m_target;

    int m_port;
    int m_priority;
    int m_weight;

    QStringList m_info;
};

class Resolver : public core::MulticastResolver
{
    Q_OBJECT
    Q_PROPERTY(QString domain READ domain WRITE setDomain NOTIFY domainChanged FINAL)

public:
    explicit Resolver(QObject *parent = {});

    QString domain() const;

public slots:
    void setDomain(QString domain);

    bool lookupHostNames(QStringList hostNames);
    bool lookupServices(QStringList serviceTypes);
    bool lookup(qnc::mdns::Message query);

signals:
    void domainChanged(QString domain);

    void hostNameFound(QString hostname, QList<QHostAddress> addresses);
    void serviceFound(qnc::mdns::ServiceDescription service);
    void messageReceived(qnc::mdns::Message message);

protected:
    [[nodiscard]] virtual quint16 port() const override;
    [[nodiscard]] QHostAddress multicastGroup(const QHostAddress &address) const override;
    void processDatagram(const QNetworkDatagram &datagram) override;

private:
    QString m_domain;
};

} // namespace qnc::mdns

QDebug operator<<(QDebug debug, const qnc::mdns::ServiceDescription &service);

#endif // QNCMDNS_MDNSRESOLVER_H
