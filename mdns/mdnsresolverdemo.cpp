/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */

// QtNetworkCrumbs headers
#include "mdnsmessage.h"
#include "mdnsresolver.h"
#include "qncliterals.h"

// Qt headers
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QTimer>

namespace qnc::mdns::demo {
namespace {

Q_LOGGING_CATEGORY(lcDemo, "mdns.demo.resolver", QtInfoMsg)

class ResolverDemo : public QCoreApplication
{
public:
    using QCoreApplication::QCoreApplication;

    int run()
    {
        const auto timeout = QCommandLineOption{{"t"_L1, "timeout"_L1},
                                                tr("Timeout after which to quit"), tr("SECONDS")};

        auto commandLine = QCommandLineParser{};
        commandLine.addOption(timeout);
        commandLine.addHelpOption();
        commandLine.process(arguments());

        if (commandLine.isSet(timeout)) {
            auto isNumber = false;
            const auto &value = commandLine.value(timeout);
            const auto seconds = value.toFloat(&isNumber);

            if (!isNumber) {
                qCWarning(lcDemo, "Not a number: %ls", qUtf16Printable(value));
                return EXIT_FAILURE;
            }

            const auto milliseconds = std::chrono::milliseconds{qRound(seconds * 1000)};
            QTimer::singleShot(milliseconds, this, &ResolverDemo::quit);
        }

        const auto resolver = new Resolver{this};

        connect(resolver, &Resolver::hostNameResolved,
                this, [](const auto hostName, const auto addresses) {
            qCInfo(lcDemo).verbosity(QDebug::MinimumVerbosity)
                    << "host resolved:" << hostName << "=>" << addresses;
        });

        connect(resolver, &Resolver::serviceResolved,
                this, [](const auto service) {
            qCInfo(lcDemo).verbosity(QDebug::MinimumVerbosity)
                    << "service resolved:" << service;
        });

        connect(resolver, &Resolver::messageReceived,
                this, [](const auto message) {
            qCDebug(lcDemo).verbosity(QDebug::MinimumVerbosity)
                    << "message received:" << message;
        });

        resolver->lookupServices({"_http._tcp"_L1, "_xpresstrain._tcp"_L1, "_googlecast._tcp"_L1});
        // resolver->lookupServices({"_tcp"_L1});
        resolver->lookupServices({"_http._tcp"_L1, "_universal"_L1});
        resolver->lookupHostNames({"juicifer"_L1, "android"_L1});

        return exec();
    }
};

} // namespace
} // namespace qnc::mdns::demo

int main(int argc, char *argv[])
{
    return qnc::mdns::demo::ResolverDemo{argc, argv}.run();
}
