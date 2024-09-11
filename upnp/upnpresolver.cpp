/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#include "upnpresolver.h"

// QtNetworkCrumbs headers
#include "httpparser.h"
#include "qncliterals.h"
#include "xmlparser.h"

// Qt headers
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace qnc::xml {

using namespace qnc::upnp;

template <> DeviceModel              &currentObject(DeviceDescription       &device)  { return device.model; }
template <> DeviceManufacturer       &currentObject(DeviceDescription       &device)  { return device.manufacturer; }
template <> IconDescription          &currentObject(DeviceDescription       &device)  { return device.icons.last(); }
template <> ServiceDescription       &currentObject(DeviceDescription       &device)  { return device.services.last(); }
template <> ActionDescription        &currentObject(ControlPointDescription &service) { return service.actions.last(); }
template <> ArgumentDescription      &currentObject(ControlPointDescription &service) { return service.actions.last().arguments.last(); }
template <> StateVariableDescription &currentObject(ControlPointDescription &service) { return service.stateVariables.last(); }
template <> ValueRangeDescription    &currentObject(ControlPointDescription &service) { return service.stateVariables.last().valueRange; }

template <> constexpr std::size_t keyCount<upnp::ArgumentDescription::Direction>     = 2;
template <> constexpr std::size_t keyCount<upnp::StateVariableDescription::DataType> = 25;

template <>
constexpr KeyValueMap<upnp::ArgumentDescription::Direction, 2> keyValueMap<>()
{
    using Direction = upnp::ArgumentDescription::Direction;

    return {
        {
            {Direction::Input,  u"in"},
            {Direction::Output, u"out"},
        }
    };
}

template <>
constexpr KeyValueMap<upnp::StateVariableDescription::DataType, 25> keyValueMap<>()
{
    using DataType = upnp::StateVariableDescription::DataType;

    return {
        {
            {DataType::Int8,            u"i1"},
            {DataType::Int16,           u"i2"},
            {DataType::Int32,           u"i4"},
            {DataType::Int64,           u"i8"},
            {DataType::UInt8,           u"ui1"},
            {DataType::UInt16,          u"ui2"},
            {DataType::UInt32,          u"ui4"},
            {DataType::UInt64,          u"ui8"},
            {DataType::Int,             u"int"},
            {DataType::Float,           u"r4"},
            {DataType::Double,          u"r8"},
            {DataType::Double,          u"number"},
            {DataType::Fixed,           u"fixed.14.4"},
            {DataType::Char,            u"char"},
            {DataType::String,          u"string"},
            {DataType::Date,            u"date"},
            {DataType::DateTime,        u"datetime"},
            {DataType::LocalDateTime,   u"datetime.tz"},
            {DataType::Time,            u"time"},
            {DataType::LocalTime,       u"time.tz"},
            {DataType::Bool,            u"boolean"},
            {DataType::Uri,             u"uri"},
            {DataType::Uuid,            u"uuid"},
            {DataType::Base64,          u"bin.base64"},
            {DataType::BinHex,          u"bin.hex"},
        }
    };
}

static_assert(keyValueMap<upnp::StateVariableDescription::DataType>().size() == 25);

} // namespace qnc::xml

namespace qnc::upnp {

namespace {

Q_LOGGING_CATEGORY(lcResolver,   "qnc.upnp.resolver")
Q_LOGGING_CATEGORY(lcParserSsdp, "qnc.upnp.parser.ssdp", QtInfoMsg)
Q_LOGGING_CATEGORY(lcParserScpd, "qnc.upnp.parser.scpd", QtInfoMsg)

static constexpr auto s_upnpDeviceNamespace  = u"urn:schemas-upnp-org:device-1-0";
static constexpr auto s_upnpServiceNamespace = u"urn:schemas-upnp-org:service-1-0";

#if QT_VERSION_MAJOR < 6
using xml::qHash;
#endif // QT_VERSION_MAJOR < 6

inline namespace states {

Q_NAMESPACE

enum class DeviceDescriptionState {
    Document,
    Root,
    SpecVersion,
    DeviceList,
    Device,
    IconList,
    Icon,
    ServiceList,
    Service,
};

Q_ENUM_NS(DeviceDescriptionState)

enum class ControlPointDescriptionState {
    Document,
    Root,
    SpecVersion,
    ActionList,
    Action,
    ArgumentList,
    Argument,
    ServiceStateTable,
    StateVariable,
    AllowedValueList,
    AllowedValueRange,
};

Q_ENUM_NS(ControlPointDescriptionState)

} // namespace states

class DeviceDescriptionParser : public xml::Parser<DeviceDescriptionState>
{
public:
    using Parser::Parser;

    [[nodiscard]] QList<DeviceDescription> read(const QUrl &deviceUrl, State initialState = State::Document);
};

class ControlPointDescriptionParser : public xml::Parser<ControlPointDescriptionState>
{
public:
    using Parser::Parser;

    [[nodiscard]] std::optional<ControlPointDescription> read(State initialState = State::Document);
};

class DetailLoader : public QObject
{
    Q_OBJECT

public:
    explicit DetailLoader(DeviceDescription &&device,
                          QNetworkAccessManager *networkAccessManager,
                          QObject *parent = nullptr)
        : QObject{parent}
        , m_networkAccessManager{networkAccessManager}
        , m_device{std::move(device)}
    {}

    void loadIcons();
    void loadServiceDescription();

    const DeviceDescription &device() const { return m_device; }
    QList<QNetworkReply *> pendingReplies() const { return m_pendingReplies; }

signals:
    void finished();

private:
    template <class Container>
    void load(const char *topic, Container *container);

    void checkIfAllRequestsFinished();

    QNetworkAccessManager *const m_networkAccessManager;
    QList<QNetworkReply *> m_pendingReplies = {};
    DeviceDescription m_device;
};

QList<DeviceDescription> DeviceDescriptionParser::read(const QUrl &deviceUrl, State initialState)
{
    auto device     = DeviceDescription{};
    device.url      = deviceUrl;
    device.baseUrl  = deviceUrl;

    auto deviceList = QList<DeviceDescription>{};
    deviceList.resize(1); // just reserving memory now to avoid dangling pointers; will update on success

    const auto parsers = StateTable {
        {
            State::Document, {
                {u"root",               transition<State::Root>()},
            }
        }, {
            State::Root, {
                {u"URLBase",            assign<&DeviceDescription::baseUrl>(device)},
                {u"specVersion",        transition<State::SpecVersion>()},
                {u"device",             transition<State::Device>()},
            }
        }, {
            State::SpecVersion, {
                {u"major",              assign<&DeviceDescription::specVersion, xml::VersionSegment::Major>(device)},
                {u"minor",              assign<&DeviceDescription::specVersion, xml::VersionSegment::Minor>(device)},
            }
        }, {
            State::DeviceList, {
                {
                    u"device", [this, &device, &deviceList] {
                        deviceList += read(device.baseUrl, State::Device);
                    }
                },
            }
        }, {
            State::Device, {
                {u"deviceType",         assign<&DeviceDescription::deviceType>(device)},
                {u"friendlyName",       assign<&DeviceDescription::displayName>(device)},
                {u"manufacturer",       assign<&DeviceManufacturer::name>(device)},
                {u"manufacturerURL",    assign<&DeviceManufacturer::url>(device)},
                {u"modelDescription",   assign<&DeviceModel::description>(device)},
                {u"modelName",          assign<&DeviceModel::name>(device)},
                {u"modelNumber",        assign<&DeviceModel::number>(device)},
                {u"modelURL",           assign<&DeviceModel::url>(device)},
                {u"presentationURL",    assign<&DeviceDescription::presentationUrl>(device)},
                {u"serialNumber",       assign<&DeviceDescription::serialNumber>(device)},
                {u"UDN",                assign<&DeviceDescription::uniqueDeviceName>(device)},
                {u"UPC",                assign<&DeviceModel::universalProductCode>(device)},

                {u"deviceList",         transition<State::DeviceList>()},
                {u"iconList",           transition<State::IconList>()},
                {u"serviceList",        transition<State::ServiceList>()},
            }
        }, {
            State::IconList, {
                {u"icon",               transition<State::Icon, &DeviceDescription::icons>(device)},
            }
        }, {
            State::Icon, {
                {u"mimetype",           assign<&IconDescription::mimeType>(device)},
                {u"width",              assign<&IconDescription::size, &QSize::setWidth>(device)},
                {u"height",             assign<&IconDescription::size, &QSize::setHeight>(device)},
                {u"depth",              assign<&IconDescription::depth>(device)},
                {u"url",                assign<&IconDescription::url>(device)},
            }
        }, {
            State::ServiceList, {
                {u"service",            transition<State::Service, &DeviceDescription::services>(device)},
            }
        }, {
            State::Service, {
                {u"serviceId",          assign<&ServiceDescription::id>(device)},
                {u"serviceType",        assign<&ServiceDescription::type>(device)},
                {u"SCPDURL",            assign<&ServiceDescription::scpdUrl>(device)},
                {u"controlURL",         assign<&ServiceDescription::controlUrl>(device)},
                {u"eventSubURL",        assign<&ServiceDescription::eventingUrl>(device)},
            }
        }
    };

    if (!Parser::parse(lcParserSsdp(), initialState, {{s_upnpDeviceNamespace, parsers}}))
        return {};

    deviceList.first() = std::move(device);

    return deviceList;
}

std::optional<ControlPointDescription> ControlPointDescriptionParser::read(State initialState)
{
    auto service = ControlPointDescription{};

    const auto parsers = StateTable {
        {
            State::Document, {
                {u"scpd",                   transition<State::Root>()},
                }
        }, {
            State::Root, {
                {u"specVersion",            transition<State::SpecVersion>()},
                {u"actionList",             transition<State::ActionList>()},
                {u"serviceStateTable",      transition<State::ServiceStateTable>()},
            }
        }, {
            State::SpecVersion, {
                {u"major",                  assign<&ControlPointDescription::spec, xml::VersionSegment::Major>(service)},
                {u"minor",                  assign<&ControlPointDescription::spec, xml::VersionSegment::Minor>(service)},
            }
        }, {
            State::ActionList, {
                {u"action",                 transition<State::Action, &ControlPointDescription::actions>(service)},
            }
        }, {
            State::Action, {
                {u"name",                   assign<&ActionDescription::name>(service)},
                {u"argumentList",           transition<State::ArgumentList>()},
                {u"Optional",               assign<&ActionDescription::flags, ActionDescription::Flag::Optional>(service)},
            }
        }, {
            State::ArgumentList, {
                {u"argument",               transition<State::Argument, &ActionDescription::arguments>(service)},
            }
        }, {
            State::Argument, {
                {u"name",                   assign<&ArgumentDescription::name>(service)},
                {u"direction",              assign<&ArgumentDescription::direction>(service)},
                {u"retval",                 assign<&ArgumentDescription::flags, ArgumentDescription::Flag::ReturnValue>(service)},
                {u"relatedStateVariable",   assign<&ArgumentDescription::stateVariable>(service)},
            }
        }, {
            State::ServiceStateTable, {
                {u"stateVariable",          transition<State::StateVariable, &ControlPointDescription::stateVariables>(service)},
            }
        }, {
            State::StateVariable, {
                {u"name",                   assign<&StateVariableDescription::name>(service)},
                {u"dataType",               assign<&StateVariableDescription::dataType>(service)},
                {u"defaultValue",           assign<&StateVariableDescription::defaultValue>(service)},
                {u"allowedValueList",       transition<State::AllowedValueList>()},
                {u"allowedValueRange",      transition<State::AllowedValueRange>()},
                {u"@sendEvents",            assign<&StateVariableDescription::flags,
                                                   StateVariableDescription::Flag::SendEvents>(service)}
            }
        }, {
            State::AllowedValueList, {
                {u"allowedValue",           append<&StateVariableDescription::allowedValues>(service)},
            }
        }, {
            State::AllowedValueRange, {
                {u"minimum",                assign<&ValueRangeDescription::minimum>(service)},
                {u"maximum",                assign<&ValueRangeDescription::maximum>(service)},
                {u"step",                   assign<&ValueRangeDescription::step>(service)},
            }
        }
    };

    if (!Parser::parse(lcParserScpd(), initialState, {{s_upnpServiceNamespace, parsers}}))
        return {};

    return service;
}

QUrl itemUrl(const IconDescription &icon)
{
    return icon.url;
}

QUrl itemUrl(const ServiceDescription &service)
{
    return service.scpdUrl;
}

bool itemHasData(const IconDescription &icon)
{
    return !icon.data.isEmpty();
}

bool itemHasData(const ServiceDescription &service)
{
    return service.scpd.has_value();
}

void updateItem(IconDescription &icon, QNetworkReply *reply)
{
    icon.data = reply->readAll();
}

void updateItem(ServiceDescription &service, QNetworkReply *reply)
{
    service.scpd = ControlPointDescription::parse(reply);
}

template <class Container>
void DetailLoader::load(const char *topic, Container *container)
{
    const auto first = container->cbegin();
    const auto last  = container->cend();

    for (auto it = first; it != last; ++it) {
        const auto &url = itemUrl(*it);

        if (url.isEmpty())
            continue;
        if (itemHasData(*it))
            continue;

        const auto request = QNetworkRequest{m_device.url.resolved(url)};
        const auto reply   = m_networkAccessManager->get(request);
        const auto index   = it - first;

        qCDebug(lcResolver,
                "Downloading %s for %ls from <%ls>", topic,
                qUtf16Printable(m_device.uniqueDeviceName),
                qUtf16Printable(request.url().toDisplayString()));

        connect(reply, &QNetworkReply::finished, this, [this, container, index, reply] {
            if (reply->error() != QNetworkReply::NoError) {
                qCWarning(lcResolver, "Could not download detail for %ls from %ls: %ls",
                          qUtf16Printable(m_device.uniqueDeviceName),
                          qUtf16Printable(reply->url().toDisplayString()),
                          qUtf16Printable(reply->errorString()));
            } else {
                updateItem((*container)[index], reply);
            }

            checkIfAllRequestsFinished();
        });

        m_pendingReplies += reply;
    }
}

void DetailLoader::loadIcons()
{
    load("icons", &m_device.icons);
}

void DetailLoader::loadServiceDescription()
{
    load("services", &m_device.services);
}

void DetailLoader::checkIfAllRequestsFinished()
{
    for (const auto reply : m_pendingReplies)
        if (reply && reply->isRunning())
            return;

    qCDebug(lcResolver,
            "All details downloaded for %ls",
            qUtf16Printable(m_device.uniqueDeviceName));

    emit finished();
}

} // namespace

Resolver::Resolver(QObject *parent)
    : ssdp::Resolver{parent}
{
    connect(this, &Resolver::serviceFound, this, &Resolver::onServiceFound);
}

Resolver::Resolver(QNetworkAccessManager *manager, QObject *parent)
    : Resolver{parent}
{
    setNetworkAccessManager(manager);
}

Resolver::~Resolver()
{
    abortPendingReplies();
}

Resolver::Behaviors Resolver::behaviors() const
{
    return m_behaviors;
}

void Resolver::setBehaviors(Behaviors newBehaviors)
{
    if (const auto oldBehaviors = std::exchange(m_behaviors, newBehaviors); oldBehaviors != newBehaviors)
        emit behaviorsChanged(m_behaviors);
}

void Resolver::setBehavior(Behavior behavior, bool enabled)
{
    auto newBehaviors = behaviors();
    newBehaviors.setFlag(behavior, enabled);
    setBehaviors(newBehaviors);
}

void Resolver::setNetworkAccessManager(QNetworkAccessManager *newManager)
{
    if (const auto oldManager = std::exchange(m_networkAccessManager, newManager); oldManager != newManager) {
        if (oldManager)
            oldManager->disconnect(this);

        abortPendingReplies();

        emit networkAccessManagerChanged(m_networkAccessManager);
    }
}

QNetworkAccessManager *Resolver::networkAccessManager() const
{
    return m_networkAccessManager;
}

void Resolver::onServiceFound(const ssdp::ServiceDescription &service)
{
    const auto &locationList = service.locations();

    for (const auto &url : locationList) {
        if (m_networkAccessManager) {
            qCDebug(lcResolver,
                    "Downloading device description for %ls from <%ls>",
                    qUtf16Printable(service.name()), qUtf16Printable(url.toDisplayString()));

            const auto reply = m_networkAccessManager->get(QNetworkRequest{url});

            connect(reply, &QNetworkReply::finished, this, [this, reply] {
                onDeviceDescriptionReceived(reply);
            });

            m_pendingReplies += reply;
        } else {
            qCDebug(lcResolver,
                    "Directly reporting %ls without downloading from <%ls>",
                    qUtf16Printable(service.name()), qUtf16Printable(url.toDisplayString()));

            auto device = DeviceDescription{};

            device.url              = url;
            device.deviceType       = service.type();
            device.uniqueDeviceName = service.name();

            emit deviceFound(device);
        }
    }
}

void Resolver::onDeviceDescriptionReceived(QNetworkReply *reply)
{
    m_pendingReplies.removeOne(reply);

    if (reply->error() == QNetworkReply::NoError) {
        qCDebug(lcResolver,
                "Device description received from <%ls>",
                qUtf16Printable(reply->url().toDisplayString()));
    } else {
        qCWarning(lcResolver,
                  "Could not download device description received from <%ls>: %ls",
                  qUtf16Printable(reply->url().toDisplayString()),
                  qUtf16Printable(reply->errorString()));
    }

    auto deviceList = DeviceDescription::parse(reply, reply->url());

    for (auto &device : deviceList) {
        if (m_behaviors && m_networkAccessManager) {
            const auto detailLoader = new DetailLoader{std::move(device), m_networkAccessManager, this};

            if (m_behaviors.testFlag(Behavior::LoadIcons))
                detailLoader->loadIcons();

            if (m_behaviors.testFlag(Behavior::LoadServiceDescription))
                detailLoader->loadServiceDescription();

            m_pendingReplies += detailLoader->pendingReplies();

            connect(detailLoader, &DetailLoader::finished,
                    this, [this, detailLoader, device] {
                emit deviceFound(detailLoader->device());
                detailLoader->deleteLater();
            });
        } else {
            emit deviceFound(device);
        }
    }
}

void Resolver::abortPendingReplies()
{
    const auto &abortedReplies = std::exchange(m_pendingReplies, {});

    for (const auto reply : abortedReplies)
        reply->abort();
}

QList<DeviceDescription> DeviceDescription::parse(QIODevice *device, const QUrl &deviceUrl)
{
    auto xml = QXmlStreamReader{device};
    return DeviceDescriptionParser{&xml}.read(deviceUrl);
}

std::optional<ControlPointDescription> ControlPointDescription::parse(QIODevice *device)
{
    auto xml = QXmlStreamReader{device};
    return ControlPointDescriptionParser{&xml}.read();
}

} // namespace qnc::upnp

QDebug operator<<(QDebug debug, const qnc::upnp::DeviceDescription &device)
{
    const auto _ = QDebugStateSaver{debug};

    const auto verbose = (debug.verbosity() > QDebug::DefaultVerbosity);
    const auto silent  = (debug.verbosity() < QDebug::DefaultVerbosity);

    if (verbose)
        debug.nospace() << device.staticMetaObject.className();

    debug << "(udn=" << device.uniqueDeviceName
          << ", type=" << device.deviceType;

    if (verbose)
        debug << ", spec=" << device.specVersion;

    if (!device.displayName.isEmpty())
        debug << ", name=" << device.displayName;

    debug << ", manufacturer=" << device.manufacturer
          << ", model=" << device.model;

    if (!device.presentationUrl.isEmpty())
        debug << ", presentationUrl=" << device.presentationUrl;
    if (!device.serialNumber.isEmpty())
        debug << ", serialNumber=" << device.serialNumber;
    if (!device.url.isEmpty())
        debug << ", url=" << device.url;

    if (!silent) {
         //     QList<IconDescription>    icons = {};
         //     QList<ServiceDescription> services = {};
    }

    return debug << ')';
}

QDebug operator<<(QDebug debug, const qnc::upnp::DeviceManufacturer &manufacturer)
{
    const auto _ = QDebugStateSaver{debug};

    const auto verbose = (debug.verbosity() > QDebug::DefaultVerbosity);
    const auto silent  = (debug.verbosity() < QDebug::DefaultVerbosity);

    if (verbose)
        debug.nospace() << manufacturer.staticMetaObject.className();

    debug << '(';

    if (!manufacturer.name.isEmpty())
        debug << ", name=" << manufacturer.name;
    if (!silent && !manufacturer.url.isEmpty())
        debug << ", url=" << manufacturer.url;

    return debug << ')';
}

QDebug operator<<(QDebug debug, const qnc::upnp::DeviceModel &model)
{
    const auto _ = QDebugStateSaver{debug};

    const auto verbose = (debug.verbosity() > QDebug::DefaultVerbosity);
    const auto silent  = (debug.verbosity() < QDebug::DefaultVerbosity);

    if (verbose)
        debug.nospace() << model.staticMetaObject.className();

    debug << '(';

    if (!model.universalProductCode.isEmpty())
        debug << ", universalProductCode=" << model.universalProductCode;
    if (!model.description.isEmpty())
        debug << ", description=" << model.description;
    if (!model.name.isEmpty())
        debug << ", name=" << model.name;
    if (!model.number.isEmpty())
        debug << ", number=" << model.number;
    if (!silent && !model.url.isEmpty())
        debug << ", url=" << model.url;

    return debug << ')';
}

#include "upnpresolver.moc"
