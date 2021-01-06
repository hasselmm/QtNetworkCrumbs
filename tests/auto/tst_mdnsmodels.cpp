/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2021 Mathias Hasselmann
 */

// MDNS headers
#include "mdnshostlistmodel.h"
#include "mdnsmessage.h"
#include "mdnsresolver.h"

// Qt headers
#include <QSignalSpy>
#include <QTest>

Q_DECLARE_METATYPE(QHostAddress)

namespace QTest {

template <>
inline bool qCompare(const QSignalSpy &t1, const QList<QVariantList> &t2,
                     const char *actual, const char *expected, const char *file, int line)
{
    return qCompare(static_cast<const QList<QVariantList> &>(t1), t2, actual, expected, file, line);
}

} // namespace QTest

namespace MDNS {
namespace Tests {

const auto s_octopiAddresses =
        "0000 0000 0000 0001 0000 0000"
        "06 6f63746f7069 05 6c6f63616c 00 0001 0001 00001194 0004 c0a8923c";
const auto s_tardisAddresses =
        "0000 0000 0000 0002 0000 0000"
        "06 746172646973 05 6c6f63616c 00 0001 0001 00000078 0004 c0a8921a"
        "c0 0c 001c 0001 00000078 0010 fe80000000000000124fa8fffe86d528";

class MockResolver : public Resolver
{
public:
    using Resolver::Resolver;
    using Resolver::parseMessage;
};

class ModelsTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void initTestCase()
    {
        qRegisterMetaType<Resolver *>();
    }

    void hostListModel()
    {
        const auto octopiName = "octopi.local";
        const auto octopiIPv4 = QHostAddress{"192.168.146.60"};
        const auto octopiAdresses = QList<QHostAddress>{octopiIPv4};
        const auto octopiTTL  = 4500;

        const auto tardisName = "tardis.local";
        const auto tardisIPv4 = QHostAddress{"192.168.146.26"};
        const auto tardisIPv6 = QHostAddress{"fe80::124f:a8ff:fe86:d528"};
        const auto tardisAddresses = QList<QHostAddress>{tardisIPv4, tardisIPv6};
        const auto tardisTTL  = 120;

        MockResolver resolver;
        HostListModel model;

        QSignalSpy resolverChanges{&model, &HostListModel::resolverChanged};
        QList<QVariantList> expectedResolverChanges;

        QSignalSpy rowsInserted{&model, &HostListModel::rowsInserted};
        QList<QVariantList> expectedRowsInserted;

        QSignalSpy dataChanged{&model, &HostListModel::dataChanged};
        QList<QVariantList> expectedDataChanged;

        // verify initial state
        QCOMPARE(model.resolver(), nullptr);
        QCOMPARE(resolverChanges, expectedResolverChanges);
        QCOMPARE(model.columnCount(), model.roleNames().size() - 1);
        QCOMPARE(model.rowCount(), 0);

        QCOMPARE(model.roleNames().value(HostListModel::DisplayRole), "display");
        QCOMPARE(model.roleNames().value(HostListModel::HostNameRole), "hostName");
        QCOMPARE(model.roleNames().value(HostListModel::AddressesRole), "addresses");

        // attach the resolver
        expectedResolverChanges.append({QVariant::fromValue(&resolver)});

        model.setResolver(&resolver);

        QCOMPARE(model.resolver(), &resolver);
        QCOMPARE(model.rowCount(), 0);

        QCOMPARE(resolverChanges, expectedResolverChanges);
        QCOMPARE(rowsInserted, expectedRowsInserted);
        QCOMPARE(dataChanged, expectedDataChanged);

        // start an initial lookup
        expectedRowsInserted.append({QModelIndex{}, 0, 0});
        QVERIFY(resolver.lookupHostNames({"tardis"}));

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(resolverChanges, expectedResolverChanges);
        QCOMPARE(rowsInserted, expectedRowsInserted);
        QCOMPARE(dataChanged, expectedDataChanged);

        QCOMPARE(model.index(0, HostListModel::HostNameColumn).data(Qt::DisplayRole), tardisName);
        QCOMPARE(model.index(0, HostListModel::HostNameColumn).data(Qt::UserRole), tardisName);
        QCOMPARE(model.index(0, HostListModel::AddressesColumn).data(Qt::DisplayRole), "");
        QCOMPARE(model.index(0, HostListModel::AddressesColumn).data(Qt::UserRole).value<QList<QHostAddress>>(), {});
        QCOMPARE(model.index(0, HostListModel::TimeToLifeColumn).data(Qt::UserRole), {});

        for (int r = 0; r < model.rowCount(); ++r)
            for (int c = 0; c < model.columnCount(); ++c)
                QCOMPARE(model.index(r, c).data(Qt::UserRole), model.index(r).data(Qt::UserRole + c));

        // receive unrelated message
        expectedRowsInserted.append({QModelIndex{}, 1, 1});
        resolver.parseMessage(Message{QByteArray::fromHex(s_octopiAddresses)});

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(resolverChanges, expectedResolverChanges);
        QCOMPARE(rowsInserted, expectedRowsInserted);
        QCOMPARE(dataChanged, expectedDataChanged);

        QCOMPARE(model.index(0, HostListModel::HostNameColumn).data(Qt::DisplayRole), tardisName);
        QCOMPARE(model.index(0, HostListModel::HostNameColumn).data(Qt::UserRole), tardisName);
        QCOMPARE(model.index(0, HostListModel::AddressesColumn).data(Qt::UserRole).value<QList<QHostAddress>>(), {});
        QCOMPARE(model.index(0, HostListModel::TimeToLifeColumn).data(Qt::UserRole), {});

        QCOMPARE(model.index(1, HostListModel::HostNameColumn).data(Qt::UserRole), octopiName);
        QCOMPARE(model.index(1, HostListModel::AddressesColumn).data(Qt::DisplayRole), octopiIPv4.toString());
        QCOMPARE(model.index(1, HostListModel::AddressesColumn).data(Qt::UserRole).value<QList<QHostAddress>>(), octopiAdresses);
        QCOMPARE(model.index(1, HostListModel::IPv4AddressColumn).data(Qt::UserRole).value<QHostAddress>(), octopiIPv4);
        QCOMPARE(model.index(1, HostListModel::IPv6AddressColumn).data(Qt::UserRole), {});
        QCOMPARE(model.index(1, HostListModel::TimeToLifeColumn).data(Qt::UserRole), octopiTTL);

        for (int r = 0; r < model.rowCount(); ++r)
            for (int c = 0; c < model.columnCount(); ++c)
                QCOMPARE(model.index(r, c).data(Qt::UserRole), model.index(r).data(Qt::UserRole + c));

        // receive requested message
        expectedDataChanged.append(QVariantList{model.index(0, HostListModel::AddressesRole),
                                                model.index(0, HostListModel::IPv4AddressColumn),
                                                QVariant::fromValue(QVector<int>{Qt::DisplayRole, Qt::UserRole})});
        resolver.parseMessage(Message{QByteArray::fromHex(s_tardisAddresses)});

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(resolverChanges, expectedResolverChanges);
        QCOMPARE(rowsInserted, expectedRowsInserted);
        qInfo() << dataChanged;
        QCOMPARE(dataChanged, expectedDataChanged);

        QCOMPARE(model.index(0, HostListModel::HostNameColumn).data(Qt::DisplayRole), tardisName);
        QCOMPARE(model.index(0, HostListModel::HostNameColumn).data(Qt::UserRole), tardisName);
        QCOMPARE(model.index(0, HostListModel::AddressesColumn).data(Qt::UserRole).value<QList<QHostAddress>>(), tardisAddresses);
        QCOMPARE(model.index(0, HostListModel::IPv4AddressColumn).data(Qt::UserRole).value<QHostAddress>(), tardisIPv4);
        QCOMPARE(model.index(0, HostListModel::IPv6AddressColumn).data(Qt::UserRole).value<QHostAddress>(), tardisIPv6);
        QCOMPARE(model.index(0, HostListModel::TimeToLifeColumn).data(Qt::UserRole), tardisTTL);

        QCOMPARE(model.index(1, HostListModel::HostNameColumn).data(Qt::UserRole), octopiName);
        QCOMPARE(model.index(1, HostListModel::AddressesColumn).data(Qt::DisplayRole), octopiIPv4.toString());
        QCOMPARE(model.index(1, HostListModel::AddressesColumn).data(Qt::UserRole).value<QList<QHostAddress>>(), {octopiIPv4});
        QCOMPARE(model.index(1, HostListModel::IPv4AddressColumn).data(Qt::UserRole).value<QHostAddress>(), octopiIPv4);
        QCOMPARE(model.index(1, HostListModel::IPv6AddressColumn).data(Qt::UserRole), {});
        QCOMPARE(model.index(1, HostListModel::TimeToLifeColumn).data(Qt::UserRole), octopiTTL);

        for (int r = 0; r < model.rowCount(); ++r)
            for (int c = 0; c < model.columnCount(); ++c)
                QCOMPARE(model.index(r, c).data(Qt::UserRole), model.index(r).data(Qt::UserRole + c));
    }

    void serviceListModel()
    {
    }
};

} // namespace Tests
} // namespace MDNS

QTEST_GUILESS_MAIN(MDNS::Tests::ModelsTest)

#include "tst_mdnsmodels.moc"
