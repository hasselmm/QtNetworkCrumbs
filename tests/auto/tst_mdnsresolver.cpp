/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */

// QtNetworkCrumbs headers
#include "mdnsresolver.h"
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
};

} // namespace qnc::mdns::tests

QTEST_GUILESS_MAIN(qnc::mdns::tests::ResolverTest)

#include "tst_mdnsresolver.moc"
