/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2021 Mathias Hasselmann
 */

// MDNS headers
#include "mdnsmessage.h"
#include "mdnsresolver.h"

// Qt headers
#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include <QLoggingCategory>

namespace {

Q_LOGGING_CATEGORY(lcDemo, "mdns.demo.resolver", QtInfoMsg)

class ResolverDemo : public QCoreApplication
{
public:
    using QCoreApplication::QCoreApplication;

    int run()
    {
        const auto resolver = new MDNS::Resolver{this};

        connect(resolver, &MDNS::Resolver::hostNameResolved,
                this, [](const auto hostName, const auto addresses) {
            qCInfo(lcDemo).verbosity(QDebug::MinimumVerbosity)
                    << "host resolved:" << hostName << "=>" << addresses;
        });

        connect(resolver, &MDNS::Resolver::serviceResolved,
                this, [](const auto service) {
            qCInfo(lcDemo).verbosity(QDebug::MinimumVerbosity)
                    << "service resolved:" << service;
        });

        connect(resolver, &MDNS::Resolver::messageReceived,
                this, [](const auto message) {
            qCDebug(lcDemo).verbosity(QDebug::MinimumVerbosity)
                    << "message received:" << message;
        });

        resolver->lookupServices({"_http._tcp", "_xpresstrain._tcp", "_googlecast._tcp"});
        resolver->lookupHostNames({"juicifer", "android"});

        return exec();
    }
};

} // namespace

int main(int argc, char *argv[])
{
    return ResolverDemo{argc, argv}.run();
}
