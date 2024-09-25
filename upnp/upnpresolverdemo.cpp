/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */

// QtNetworkCrumbs headers
#include "qncliterals.h"
#include "upnpresolver.h"

// Qt headers
#include <QCoreApplication>
#include <QLoggingCategory>

namespace qnc::upnp::demo {
namespace {

Q_LOGGING_CATEGORY(lcDemo, "upnp.demo.resolver", QtInfoMsg)

class ResolverDemo : public QCoreApplication
{
public:
    using QCoreApplication::QCoreApplication;

    int run()
    {
        const auto resolver = new Resolver{this};
        resolver->setNetworkAccessManager(new QNetworkAccessManager{this});

        connect(resolver, &Resolver::deviceFound, this, [resolver](const auto &device) {
            qCInfo(lcDemo).verbosity(QDebug::MinimumVerbosity)
                    << "device found:"
                    << device.displayName
                    << device.deviceType;

            const auto saveUrl = [device, resolver](const QString &subDir, QUrl url) {
                if (url.isEmpty())
                    return;

                if (url.isRelative())
                    url = device.url.resolved(url); // FIXME: also deprecated baseUrl somehow

                qInfo(lcDemo) << subDir << url;

#if 0 // FIXME: make this a behavior
                const auto request = QNetworkRequest{url};
                const auto reply = resolver->networkAccessManager()->get(request);
                connect(reply, &QNetworkReply::finished, resolver, [reply, subDir] {
                    reply->deleteLater();

                    if (reply->error() != QNetworkReply::NoError) {
                        qCWarning(lcDemo) << subDir << reply->url() << "failed:" << reply->errorString();
                        return;
                    }

                    qInfo() << "GOOD!" << reply->url();

                    if (auto dir = QDir::current(); !dir.mkpath(subDir)) {
                        qCWarning(lcDemo) << "Could not create dir" << subDir;
                        return;
                    }

                    const auto &fileName = QString::fromLatin1(QUrl::toPercentEncoding(reply->url().toString()));
                    if (auto file = QFile{QDir{subDir}.filePath(fileName)}; !file.open(QFile::WriteOnly)) {
                        qCWarning(lcDemo) << "Coudl not create file:" << file.fileName() << file.errorString();
                    } else {
                        qInfo() << "WRITE!" << file.fileName() << reply->url();
                        qInfo() << "ABS:" << QFileInfo{file.fileName()}.absoluteFilePath();
                        file.write(reply->readAll());
                    }
                });
#endif
            };

            for (const auto &icon : device.icons)
                saveUrl("upnp/icons"_L1, icon.url);

            for (const auto &service : device.services) {
                saveUrl("upnp/scpd"_L1, service.scpdUrl);
                saveUrl("upnp/control"_L1, service.controlUrl);
                saveUrl("upnp/eventing"_L1, service.eventingUrl);
            }
        });

        connect(resolver, &Resolver::serviceLost,
                this, [](const auto &serviceName) {
            qCInfo(lcDemo).verbosity(QDebug::MinimumVerbosity)
                    << "service lost:"
                    << serviceName;
        });

        resolver->lookupService("upnp:rootdevice"_L1);
        /*
        resolver->lookupService("urn:schemas-upnp-org:device:MediaRenderer:1"_L1);
        resolver->lookupService("urn:schemas-sony-com:service:IRCC:1"_L1);
        resolver->lookupService("urn:schemas-sony-com:service:ScalarWebAPI:1"_L1);
         */
        return exec();
    }
};

} // namespace
} // namespace qnc::upnp::demo

int main(int argc, char *argv[])
{
    return qnc::upnp::demo::ResolverDemo{argc, argv}.run();
}
