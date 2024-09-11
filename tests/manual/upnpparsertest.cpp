/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */

// QtNetworkCrumbs headers
#include "qncliterals.h"
#include "upnpresolver.h"

// Qt headers
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>

namespace qnc::upnp::tests {
namespace {

class ParserTest : public QCoreApplication
{
public:
    using QCoreApplication::QCoreApplication;

    int run()
    {
        const auto ssdpDir     = QCommandLineOption{"ssdp"_L1,     tr("directory with SSDP files"),     tr("DIRPATH")};
        const auto scpdDir     = QCommandLineOption{"scpd"_L1,     tr("directory with SCPD files"),     tr("DIRPATH")};
        const auto controlDir  = QCommandLineOption{"control"_L1,  tr("directory with control files"),  tr("DIRPATH")};
        const auto eventingDir = QCommandLineOption{"eventing"_L1, tr("directory with eventing files"), tr("DIRPATH")};

        auto commandLine = QCommandLineParser{};

        commandLine.addOption(ssdpDir);
        commandLine.addOption(scpdDir);
        commandLine.addOption(controlDir);
        commandLine.addOption(eventingDir);
        commandLine.addHelpOption();

        commandLine.process(arguments());

        if (!commandLine.positionalArguments().isEmpty()) {
            commandLine.showHelp(EXIT_FAILURE);
            return EXIT_FAILURE;
        }

        if (commandLine.isSet(ssdpDir) && !readTestData(commandLine.value(ssdpDir), &ssdpParse))
            return EXIT_FAILURE;
        if (commandLine.isSet(scpdDir) && !readTestData(commandLine.value(scpdDir), &scpdParse))
            return EXIT_FAILURE;

        return EXIT_SUCCESS;
    }

private:
    static bool readTestData(const QString &path, bool (* parse)(QFile *))
    {
        const auto fileInfo = QFileInfo{path};

        if (fileInfo.isDir()) {
            const auto &fileInfoList = QDir{path}.entryInfoList(QDir::Files);

            for (const auto &fileInfo : fileInfoList) {
                if (!readFile(fileInfo, parse))
                    return false;
            }

            return true;
        } else {
            return readFile(fileInfo, parse);
        }
    }

    static bool readFile(const QFileInfo &fileInfo, bool (* parse)(QFile *))
    {
        qInfo() << QUrl::fromPercentEncoding(fileInfo.fileName().toUtf8());

        auto file = QFile{fileInfo.filePath()};
        qInfo() << file.fileName();

        if (!file.open(QFile::ReadOnly)) {
            qWarning() << file.errorString();
            return false;
        }

        if (!parse(&file))
            return false;

        return true;
    }

    static bool ssdpParse(QFile *file)
    {
        const auto &deviceList = DeviceDescription::parse(file);

        if (deviceList.isEmpty())
            return false;

        for (const auto &device : deviceList) {
            qInfo() << "-"
                    << device.specVersion
                    << device.displayName
                    << device.deviceType;

            for (const auto &service : device.services) {
                qInfo() << " -"
                        << service.type
                        << service.id;
            }
        }

        return true;
    }

    static bool scpdParse(QFile *file)
    {
        const auto controlPoint = ControlPointDescription::parse(file);

        if (!controlPoint)
            return false;

        qInfo() << "- actions:";
        for (const auto &action : controlPoint->actions) {
            qInfo() << " -"
                    << action.name
                    << action.flags;

            for (const auto &argument : action.arguments) {
                qInfo() << "  -"
                        << argument.name
                        // FIXME: << argument.direction
                        << argument.stateVariable;
            }
        }

        qInfo() << "- variables:";
        for (const auto &variable : controlPoint->stateVariables) {
            qInfo() << " -"
                    << variable.name
                    // FIXME: << variable.dataType
                    << variable.defaultValue
                    << variable.flags
                    << variable.allowedValues
                    << variable.valueRange.minimum
                    << variable.valueRange.maximum
                    << variable.valueRange.step;
        }

        return true;
    }
};

} // namespace
} // namespace qnc::upnp::tests

int main(int argc, char *argv[])
{
    return qnc::upnp::tests::ParserTest{argc, argv}.run();
}

