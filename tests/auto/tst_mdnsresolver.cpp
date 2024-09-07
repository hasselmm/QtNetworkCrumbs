/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */

// QtNetworkCrumbs headers
#include "mdnsresolver.h"

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

using namespace std::chrono_literals;

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

        QCOMPARE(resolver.domain(), "local");
        QCOMPARE(domainChanges, expectedDomainChanges);

        resolver.setDomain("test");

        expectedDomainChanges += {"test"};
        QCOMPARE(resolver.domain(), "test");
        QCOMPARE(domainChanges, expectedDomainChanges);

        resolver.setDomain("test");

        QCOMPARE(resolver.domain(), "test");
        QCOMPARE(domainChanges, expectedDomainChanges);

        resolver.setDomain("local");

        expectedDomainChanges += {"local"};
        QCOMPARE(resolver.domain(), "local");
        QCOMPARE(domainChanges, expectedDomainChanges);
    }

    void intervalProperty()
    {
        auto resolver = Resolver{};
        auto intervalChanges = QSignalSpy{&resolver, &Resolver::scanIntervalChanged};
        auto expectedIntervalChanges = QList<QVariantList>{};

        QCOMPARE(resolver.scanInterval(), 2000);
        QCOMPARE(resolver.scanIntervalAsDuration(), 2s);
        QCOMPARE(intervalChanges, expectedIntervalChanges);

        resolver.setScanInterval(2s);

        QCOMPARE(resolver.scanInterval(), 2000);
        QCOMPARE(resolver.scanIntervalAsDuration(), 2s);
        QCOMPARE(intervalChanges, expectedIntervalChanges);

        resolver.setScanInterval(3s);

        expectedIntervalChanges += QVariantList{3000};
        QCOMPARE(resolver.scanInterval(), 3000);
        QCOMPARE(resolver.scanIntervalAsDuration(), 3s);
        QCOMPARE(intervalChanges, expectedIntervalChanges);

        resolver.setScanInterval(3000);

        QCOMPARE(resolver.scanInterval(), 3000);
        QCOMPARE(resolver.scanIntervalAsDuration(), 3s);
        QCOMPARE(intervalChanges, expectedIntervalChanges);

        resolver.setScanInterval(3500);

        expectedIntervalChanges += QVariantList{3500};
        QCOMPARE(resolver.scanInterval(), 3500);
        QCOMPARE(resolver.scanIntervalAsDuration(), 3500ms);
        QCOMPARE(intervalChanges, expectedIntervalChanges);
    }

    void lookupHostNames()
    {
        auto resolver = Resolver{};

        QVERIFY(resolver.lookupHostNames({"alpha"}));
        QVERIFY(resolver.lookupHostNames({"alpha", "beta"}));
        QVERIFY(!resolver.lookupHostNames({"alpha", "beta"}));
        QVERIFY(resolver.lookupHostNames({"beta"})); // FIXME: do we want to merge this into the previous query?

        // verify search domain gets removed
        QCOMPARE(resolver.domain(), "local");
        QVERIFY(!resolver.lookupHostNames({"beta.local"}));
        QVERIFY(!resolver.lookupHostNames({"beta.local."}));
    }

    void lookupServices()
    {
        auto resolver = Resolver{};

        QVERIFY(resolver.lookupServices({"_http._tcp"}));
        QVERIFY(resolver.lookupServices({"_http._tcp", "_ipp._tcp"}));
        QVERIFY(!resolver.lookupServices({"_http._tcp", "_ipp._tcp"}));
        QVERIFY(resolver.lookupServices({"_ipp._tcp"})); // FIXME: do we want to merge this into the previous query?

        // verify search domain gets removed
        QCOMPARE(resolver.domain(), "local");
        QVERIFY(!resolver.lookupServices({"_ipp._tcp.local"}));
        QVERIFY(!resolver.lookupServices({"_ipp._tcp.local."}));
    }
};

} // namespace qnc::mdns::tests

QTEST_GUILESS_MAIN(qnc::mdns::tests::ResolverTest)

#include "tst_mdnsresolver.moc"
