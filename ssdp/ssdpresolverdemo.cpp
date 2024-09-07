/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */

// QtNetworkCrumbs headers
#include "qncliterals.h"
#include "ssdpresolver.h"

// Qt headers
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QUuid>

namespace qnc::ssdp::demo {
namespace {

Q_LOGGING_CATEGORY(lcDemo, "ssdp.demo.resolver", QtInfoMsg)

class ResolverDemo : public QCoreApplication
{
public:
    using QCoreApplication::QCoreApplication;

    int run()
    {
        const auto resolver = new Resolver{this};

        connect(resolver, &Resolver::serviceFound,
                this, [](const auto &service) {
            qCInfo(lcDemo).verbosity(QDebug::MinimumVerbosity)
                    << "service resolved:"
                    << service;
        });

        connect(resolver, &Resolver::serviceLost,
                this, [](const auto &serviceName) {
            qCInfo(lcDemo).verbosity(QDebug::MinimumVerbosity)
                    << "service lost:"
                    << serviceName;
        });

        resolver->lookupService("ssdp:all"_L1);

        return exec();
    }
};

} // namespace
} // namespace qnc::ssdp::demo

int main(int argc, char *argv[])
{
    return qnc::ssdp::demo::ResolverDemo{argc, argv}.run();
}
