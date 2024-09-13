/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */

// QtNetworkCrumbs headers
#include "mdnsmessage.h"
#include "mdnsresolver.h"
#include "mdnsurlfinder.h"
#include "qncliterals.h"

// Qt headers
#include <QSignalSpy>
#include <QTest>

namespace QTest {

template <>
inline bool qCompare(const QSignalSpy &t1, const QList<QVariantList> &t2,
                     const char *actual, const char *expected, const char *file, int line)
{
    return qCompare(static_cast<const QList<QVariantList> &>(t1), t2, actual, expected, file, line);
}

} // namespace QTest

Q_DECLARE_METATYPE(qnc::mdns::ServiceDescription)

namespace qnc::mdns::tests {

using namespace std::chrono;
using namespace std::chrono_literals;

namespace {

template<typename... Ts>
constexpr int ms(const duration<Ts...> &duration)
{
    return static_cast<int>(duration_cast<milliseconds>(duration).count());
}

} // namespace

class ResolverTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void domainProperty()
    {
        auto resolver = Resolver{};
        auto domainChanges = QSignalSpy{&resolver, &Resolver::domainChanged};
        auto expectedDomainChanges = QList<QVariantList>{};

        const auto s_local = "local"_L1;
        const auto s_test  = "test"_L1;

        QCOMPARE(resolver.domain(), s_local);
        QCOMPARE(domainChanges, expectedDomainChanges);

        resolver.setDomain(s_test);

        expectedDomainChanges += {s_test};
        QCOMPARE(resolver.domain(), s_test);
        QCOMPARE(domainChanges, expectedDomainChanges);

        resolver.setDomain(s_test);

        QCOMPARE(resolver.domain(), s_test);
        QCOMPARE(domainChanges, expectedDomainChanges);

        resolver.setDomain(s_local);

        expectedDomainChanges += {s_local};
        QCOMPARE(resolver.domain(), s_local);
        QCOMPARE(domainChanges, expectedDomainChanges);
    }

    void intervalProperty()
    {
        auto resolver = Resolver{};
        auto intervalChanges = QSignalSpy{&resolver, &Resolver::scanIntervalChanged};
        auto expectedIntervalChanges = QList<QVariantList>{};

        constexpr auto interval0 = 15s;
        constexpr auto interval1 = 3s;
        constexpr auto interval2 = 3500ms;

        QCOMPARE(resolver.scanInterval(), ms(interval0));
        QCOMPARE(resolver.scanIntervalAsDuration(), interval0);
        QCOMPARE(intervalChanges, expectedIntervalChanges);

        resolver.setScanInterval(interval0);

        QCOMPARE(resolver.scanInterval(), ms(interval0));
        QCOMPARE(resolver.scanIntervalAsDuration(), interval0);
        QCOMPARE(intervalChanges, expectedIntervalChanges);

        resolver.setScanInterval(interval1);

        expectedIntervalChanges += QVariantList{ms(interval1)};
        QCOMPARE(resolver.scanInterval(), ms(interval1));
        QCOMPARE(resolver.scanIntervalAsDuration(), interval1);
        QCOMPARE(intervalChanges, expectedIntervalChanges);

        resolver.setScanInterval(ms(interval1));

        QCOMPARE(resolver.scanInterval(), ms(interval1));
        QCOMPARE(resolver.scanIntervalAsDuration(), interval1);
        QCOMPARE(intervalChanges, expectedIntervalChanges);

        resolver.setScanInterval(ms(interval2));

        expectedIntervalChanges += QVariantList{ms(interval2)};
        QCOMPARE(resolver.scanInterval(), ms(interval2));
        QCOMPARE(resolver.scanIntervalAsDuration(), interval2);
        QCOMPARE(intervalChanges, expectedIntervalChanges);
    }

    void lookupHostNames()
    {
        auto resolver = Resolver{};

        constexpr auto hostName1 = "alpha"_L1;
        constexpr auto hostName2 = "beta"_L1;

        QVERIFY(resolver.lookupHostNames({hostName1}));
        QVERIFY(resolver.lookupHostNames({hostName1, hostName2}));
        QVERIFY(!resolver.lookupHostNames({hostName1, hostName2}));
        QVERIFY(resolver.lookupHostNames({hostName2})); // FIXME: do we want to merge this into the previous query?

        // verify search domain gets removed
        QCOMPARE(resolver.domain(), "local"_L1);
        QVERIFY(!resolver.lookupHostNames({"beta.local"_L1}));
        QVERIFY(!resolver.lookupHostNames({"beta.local."_L1}));
    }

    void lookupServices()
    {
        auto resolver = Resolver{};

        constexpr auto serviceType1 = "_http._tcp"_L1;
        constexpr auto serviceType2 = "_ipp._tcp"_L1;

        QVERIFY(resolver.lookupServices({serviceType1}));
        QVERIFY(resolver.lookupServices({serviceType1, serviceType2}));
        QVERIFY(!resolver.lookupServices({serviceType1, serviceType2}));
        QVERIFY(resolver.lookupServices({serviceType2})); // FIXME: do we want to merge this into the previous query?

        // verify search domain gets removed
        QCOMPARE(resolver.domain(), "local"_L1);
        QVERIFY(!resolver.lookupServices({"_ipp._tcp.local"_L1}));
        QVERIFY(!resolver.lookupServices({"_ipp._tcp.local."_L1}));
    }

    void serviceLocations_data()
    {
        QTest::addColumn<ServiceDescription>("service");
        QTest::addColumn<QString>           ("expectedType");
        QTest::addColumn<QString>           ("expectedTarget");
        QTest::addColumn<int>               ("expectedPort");
        QTest::addColumn<QList<QUrl>>       ("expectedLocations");

        const auto httpRecord    = ServiceRecord{"0000|0000|0050|06<66 72 69 64 67 65>00"_hex, 0};
        const auto httpsRecord   = ServiceRecord{"0000|0000|01bb|06<66 72 69 64 67 65>00"_hex, 0};
        const auto mqttRecord    = ServiceRecord{"0000|0000|075b|07<69 6f 74 73 69 6e 6b>00"_hex, 0};
        const auto printerRecord = ServiceRecord{"0000|0000|0277|07<70 72 69 6e 74 65 72>00"_hex, 0};
        const auto customRecord  = ServiceRecord{"0000|0000|affe|06<63 75 73 74 6f 6d>00"_hex, 0};
        const auto unknownRecord = ServiceRecord{"0000|0000|7353|06<6a 61 71 75 65 73>00"_hex, 0};

        QTest::newRow("http-default")
                << ServiceDescription{"local"_L1, "fridge._http._tcp.local.",
                   httpRecord, {"path=/webapp/"_L1}}
                << "_http._tcp" << "fridge.local" << 80
                << QList{"http://fridge.local/webapp/"_url};

        QTest::newRow("http-custom")
                << ServiceDescription{"local"_L1, "fridge._http._tcp.local.",
                   httpsRecord, {"path=/webapp"_L1, "u=test"_L1}}
                << "_http._tcp" << "fridge.local" << 443
                << QList{"http://test@fridge.local:443/webapp"_url};

        QTest::newRow("https-default")
                << ServiceDescription{"local"_L1, "fridge._https._tcp.local.",
                   httpsRecord, {"path=webapp"_L1}}
                << "_https._tcp" << "fridge.local" << 443
                << QList{"https://fridge.local/webapp"_url};

        QTest::newRow("https-custom")
                << ServiceDescription{"local"_L1, "fridge._https._tcp.local.",
                   httpRecord, {"u=user"_L1, "p=secret"_L1}}
                << "_https._tcp" << "fridge.local" << 80
                << QList{"https://user:secret@fridge.local:80/"_url};

        QTest::newRow("mqtt")
                << ServiceDescription{"local"_L1, "iotsink._mqtt._tcp.local.",
                   mqttRecord, {"topic=test"_L1}}
                << "_mqtt._tcp" << "iotsink.local" << 1883
                << QList{"mqtt://iotsink.local/test"_url};

        QTest::newRow("ipp")
                << ServiceDescription{"local"_L1, "printer._ipp._tcp.local.", printerRecord, {
                   "rp=ipp"_L1, "adminurl=https://printer.local/"_L1,
                    "DUUID=8e17611f-95d7-4398-a588-1a8f1438c3ba"_L1}}
                << "_ipp._tcp" << "printer.local" << 631
                << QList{"ipp://printer.local/ipp"_url,
                   "https://printer.local/"_url,
                   "urn:uuid:8e17611f-95d7-4398-a588-1a8f1438c3ba"_url};

        QTest::newRow("custom")
                << ServiceDescription{"local"_L1, "custom._marche._tcp.local.",
                   customRecord, {"fruits=cherries"_L1}}
                << "_marche._tcp" << "custom.local" << 45054
                << QList{"marche://custom.local/cherries"_url};

        QTest::newRow("unknown")
                << ServiceDescription{"local"_L1, "jaques._inconnu._tcp.local.",
                   unknownRecord, {"legume=chou"_L1}}
                << "_inconnu._tcp" << "jaques.local" << 29523
                << QList<QUrl>{};

        UrlFinder::add("_marche._tcp"_L1, [](const auto &) {
            return QList{"marche://custom.local/cherries"_url};
        });
    }

    void serviceLocations()
    {
        const QFETCH(ServiceDescription, service);
        const QFETCH(QString,            expectedType);
        const QFETCH(QString,            expectedTarget);
        const QFETCH(int,                expectedPort);
        const QFETCH(QList<QUrl>,        expectedLocations);

        QCOMPARE(service.type(),      expectedType);
        QCOMPARE(service.target(),    expectedTarget);
        QCOMPARE(service.port(),      expectedPort);
        QCOMPARE(service.locations(), expectedLocations);
    }
};

} // namespace qnc::mdns::tests

QTEST_GUILESS_MAIN(qnc::mdns::tests::ResolverTest)

#include "tst_mdnsresolver.moc"
