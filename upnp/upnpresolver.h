/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#ifndef QNCUPNP_UPNPRESOLVER_H
#define QNCUPNP_UPNPRESOLVER_H

// QtNetworkCrumbs headers
#include "ssdpresolver.h"

// Qt headers
#include <QNetworkAccessManager>
#include <QSize>
#include <QVersionNumber>

class QXmlStreamReader;

namespace qnc::upnp {

struct IconDescription
{
    QString         mimeType = {};
    QSize           size     = {};
    int             depth    = {};
    QUrl            url      = {};
    QByteArray      data     = {};

    Q_GADGET
};

inline namespace scpd {

struct ArgumentDescription
{
    enum class Direction {
        Input,
        Output,
    };

    Q_ENUM(Direction)

    enum class Flag {
        ReturnValue = 1 << 0,
    };

    Q_FLAG(Flag)
    Q_DECLARE_FLAGS(Flags, Flag)

    QString   name          = {};
    Direction direction     = Direction::Output;
    Flags     flags         = {};
    QString   stateVariable = {};

    Q_GADGET
};

struct ActionDescription
{
    enum class Flag {
        Optional = 1 << 0,
    };

    Q_FLAG(Flag)
    Q_DECLARE_FLAGS(Flags, Flag)

    QString                    name      = {};
    Flags                      flags     = {};
    QList<ArgumentDescription> arguments = {};

    Q_GADGET
};

struct ValueRangeDescription
{
    // FIXME: instead of resorting to qint64 this must depend on dataType; at least it should be a variant

    qint64 minimum = {};
    qint64 maximum = {};
    qint64 step    = {};

    Q_GADGET
};

struct StateVariableDescription
{
    enum class Flag {
        SendEvents = 1 << 0,
    };

    Q_FLAG(Flag)
    Q_DECLARE_FLAGS(Flags, Flag)

    enum class DataType { // FIXME obey Qt naming conventions
        Int8,
        Int16,
        Int32,
        Int64,

        UInt8,
        UInt16,
        UInt32,
        UInt64,

        Int,
        Float,
        Double,
        Fixed,

        Char,
        String,

        Date,
        DateTime,
        LocalDateTime,

        Time,
        LocalTime,

        Bool,
        Uri,
        Uuid,

        Base64,
        BinHex,

        I1          = Int8,
        I2          = Int16,
        I4          = Int32,
        I8          = Int64,

        UI1         = UInt8,
        UI2         = UInt16,
        UI4         = UInt32,
        UI8         = UInt64,

        R4          = Float,
        R8          = Double,
        Number      = Double,
    };

    Q_ENUM(DataType)

    using DataTypeVariant = std::variant<std::monostate, DataType, QString>;

    QString               name;
    Flags                 flags;
    DataTypeVariant       dataType;
    QString               defaultValue;
    QStringList           allowedValues;    // FIXME: make optional
    ValueRangeDescription valueRange;       // FIXME: make optional

    Q_GADGET
};

struct ControlPointDescription
{
    QVersionNumber                  spec           = {};
    QList<ActionDescription>        actions        = {};
    QList<StateVariableDescription> stateVariables = {};

    [[nodiscard]] static std::optional<ControlPointDescription> parse(QIODevice *device);

    Q_GADGET
};

} // inline namespace scpd

struct ServiceDescription
{
    QString         id          = {};
    QString         type        = {};
    QUrl            scpdUrl     = {};
    QUrl            controlUrl  = {};
    QUrl            eventingUrl = {};

    std::optional<ControlPointDescription> scpd = {};

    Q_GADGET
};

struct DeviceManufacturer
{
    QString name = {};
    QUrl    url  = {};

    Q_GADGET
};

struct DeviceModel
{
    QString description          = {};
    QString name                 = {};
    QString number               = {};
    QUrl    url                  = {};
    QString universalProductCode = {};

    Q_GADGET
};

struct DeviceDescription
{
    QUrl               url               = {};
    QUrl               baseUrl           = {};
    QVersionNumber     specVersion       = {};
    QString            uniqueDeviceName  = {};
    QString            deviceType        = {};
    QString            displayName       = {};
    DeviceManufacturer manufacturer      = {};
    DeviceModel        model             = {};
    QUrl               presentationUrl   = {};
    QString            serialNumber      = {};

    QList<IconDescription>    icons      = {};
    QList<ServiceDescription> services   = {};

    [[nodiscard]] static QList<DeviceDescription> parse(QIODevice *device, const QUrl &deviceUrl = {});

    Q_GADGET
};

class Resolver : public ssdp::Resolver
{
    Q_OBJECT
    Q_PROPERTY(Behaviors behaviors READ behaviors
               WRITE setBehaviors NOTIFY behaviorsChanged FINAL)
    Q_PROPERTY(QNetworkAccessManager *networkAccessManager READ networkAccessManager
               WRITE setNetworkAccessManager NOTIFY networkAccessManagerChanged FINAL)

public:
    enum class Behavior
    {
        LoadIcons              = (1 << 0),
        LoadServiceDescription = (1 << 1),
    };

    Q_FLAG(Behavior)
    Q_DECLARE_FLAGS(Behaviors, Behavior)

    explicit Resolver(QObject *parent = nullptr);
    explicit Resolver(QNetworkAccessManager *manager, QObject *parent = nullptr);
    virtual ~Resolver();

    [[nodiscard]] Behaviors behaviors() const;
    [[nodiscard]] QNetworkAccessManager *networkAccessManager() const;

public slots:
    void setBehaviors(Behaviors newBehaviors);
    void setBehavior(Behavior behavior, bool enable = true);
    void setNetworkAccessManager(QNetworkAccessManager *newManager);

signals:
    void behaviorsChanged(Behaviors behaviors);
    void networkAccessManagerChanged(QNetworkAccessManager *manager);
    void deviceFound(const qnc::upnp::DeviceDescription &device);

private:
    void onServiceFound(const ssdp::ServiceDescription &service);
    void onDeviceDescriptionReceived(QNetworkReply *reply);
    void abortPendingReplies();

    QPointer<QNetworkAccessManager> m_networkAccessManager = {};
    QList<QNetworkReply *>          m_pendingReplies = {};
    Behaviors                       m_behaviors = {};
};

} // namespace qnc::upnp

QDebug operator<<(QDebug debug, const qnc::upnp::DeviceDescription  &device);
QDebug operator<<(QDebug debug, const qnc::upnp::DeviceManufacturer &manufacturer);
QDebug operator<<(QDebug debug, const qnc::upnp::DeviceModel        &model);
/*
QDebug operator<<(QDebug debug, const qnc::upnp::IconDescription    &icon);
QDebug operator<<(QDebug debug, const qnc::upnp::ServiceDescription &service);
 */

#endif // QNCUPNP_UPNPRESOLVER_H
