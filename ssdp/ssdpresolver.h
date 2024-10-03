/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#ifndef QNCSSDP_RESOLVER_H
#define QNCSSDP_RESOLVER_H

#include "multicastresolver.h"

#include <QDateTime>
#include <QPointer>
#include <QUrl>

namespace qnc::ssdp {

class ServiceDescription
{
    Q_GADGET
    Q_PROPERTY(QString     name                 READ name                 CONSTANT FINAL)
    Q_PROPERTY(QString     type                 READ type                 CONSTANT FINAL)
    Q_PROPERTY(QList<QUrl> locations            READ locations            CONSTANT FINAL)
    Q_PROPERTY(QList<QUrl> alternativeLocations READ alternativeLocations CONSTANT FINAL)
    Q_PROPERTY(QDateTime   expires              READ expires              CONSTANT FINAL)

public:
    ServiceDescription(const QString     &name,
                       const QString     &type,
                       const QList<QUrl> &locations,
                       const QList<QUrl> &alternativeLocations,
                       const QDateTime   &expires)
        : m_name                {name}
        , m_type                {type}
        , m_locations           {locations}
        , m_alternativeLocations{alternativeLocations}
        , m_expires             {expires}
    {}


    QString     name()                 const { return m_name; }
    QString     type()                 const { return m_type; }
    QList<QUrl> locations()            const { return m_locations; }
    QList<QUrl> alternativeLocations() const { return m_alternativeLocations; }
    QDateTime   expires()              const { return m_expires; }

private:
    QString     m_name;
    QString     m_type;
    QList<QUrl> m_locations;
    QList<QUrl> m_alternativeLocations;
    QDateTime   m_expires;
};

struct NotifyMessage
{
    enum class Type {
        Invalid,
        Alive,
        ByeBye,
    };

    Q_ENUM(Type)

    Type        type         = Type::Invalid;
    QString     serviceName  = {};
    QString     serviceType  = {};
    QList<QUrl> locations    = {};
    QList<QUrl> altLocations = {};
    QDateTime   expiry       = {};

    static NotifyMessage parse(const QByteArray &data, const QDateTime &now);
    static NotifyMessage parse(const QByteArray &data);

    Q_GADGET
};

struct ServiceLookupRequest
{
    using seconds = std::chrono::seconds;

    QString serviceType;
    seconds minimumDelay = seconds{0};
    seconds maximumDelay = seconds{5};
};

class Resolver : public core::MulticastResolver
{
    Q_OBJECT

public:
    using core::MulticastResolver::MulticastResolver;

public slots:
    bool lookupService(const ServiceLookupRequest &request);
    bool lookupService(const QString &serviceType);

signals:
    void serviceFound(const qnc::ssdp::ServiceDescription &service);
    void serviceLost(const QString &uniqueServiceName);

protected:
    [[nodiscard]] quint16 port() const override;
    [[nodiscard]] QHostAddress multicastGroup(const QHostAddress &address) const override;
    [[nodiscard]] QByteArray finalizeQuery(const QHostAddress &address, const QByteArray &query) const override;

    void processDatagram(const QNetworkDatagram &message) override;
};

} // namespace qnc::ssdp

QDebug operator<<(QDebug debug, const qnc::ssdp::NotifyMessage      &message);
QDebug operator<<(QDebug debug, const qnc::ssdp::ServiceDescription &service);

#endif // QNCSSDP_RESOLVER_H
