/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#ifndef QNCMDNS_MDNSURLFINDER_H
#define QNCMDNS_MDNSURLFINDER_H

#include <QString>
#include <QUrl>

namespace qnc::mdns {

class ServiceDescription;

class UrlFinder
{
public:
    using Function = std::function<QList<QUrl>(const ServiceDescription &)>;
    static void add(const QString &serviceType, const Function &finder);
    static QList<QUrl> find(const ServiceDescription &service);

    QList<QUrl> operator()(const ServiceDescription &service) const { return run(service); }

private:
    virtual QList<QUrl> run(const ServiceDescription &service) const = 0;
};

class DefaultUrlFinder : public UrlFinder
{
public:
    DefaultUrlFinder(QStringView urlScheme, int defaultPort, QStringView pathKey = {})
        : m_urlScheme  {std::move(urlScheme)}
        , m_defaultPort{std::move(defaultPort)}
        , m_pathKey    {std::move(pathKey)}
    {}

protected:
    QList<QUrl> run(const ServiceDescription &service) const override;

private:
    QStringView m_urlScheme   = {};
    int         m_defaultPort = {};
    QStringView m_pathKey     = {};
};

class PrinterUrlFinder : public DefaultUrlFinder
{
public:
    using DefaultUrlFinder::DefaultUrlFinder;

protected:
    QList<QUrl> run(const ServiceDescription &service) const override;
};

} // namespace qnc::mdns

#endif // QNCMDNS_MDNSURLFINDER_H
