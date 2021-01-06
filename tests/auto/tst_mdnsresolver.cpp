/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2021 Mathias Hasselmann
 */

// MDNS headers
#include "mdnsmessage.h"
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

namespace MDNS {
namespace Tests {

using namespace std::chrono_literals;

class ResolverTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void domainProperty()
    {
        Resolver resolver;
        QSignalSpy domainChanges{&resolver, &Resolver::domainChanged};
        QList<QVariantList> expectedDomainChanges;

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
        Resolver resolver;
        QSignalSpy intervalChanges{&resolver, &Resolver::intervalChanged};
        QList<QVariantList> expectedIntervalChanges;

        QCOMPARE(resolver.interval(), 2000);
        QCOMPARE(resolver.intervalAsDuration(), 2s);
        QCOMPARE(intervalChanges, expectedIntervalChanges);

        resolver.setInterval(2s);

        QCOMPARE(resolver.interval(), 2000);
        QCOMPARE(resolver.intervalAsDuration(), 2s);
        QCOMPARE(intervalChanges, expectedIntervalChanges);

        resolver.setInterval(3s);

        expectedIntervalChanges += {3000};
        QCOMPARE(resolver.interval(), 3000);
        QCOMPARE(resolver.intervalAsDuration(), 3s);
        QCOMPARE(intervalChanges, expectedIntervalChanges);

        resolver.setInterval(3000);

        QCOMPARE(resolver.interval(), 3000);
        QCOMPARE(resolver.intervalAsDuration(), 3s);
        QCOMPARE(intervalChanges, expectedIntervalChanges);

        resolver.setInterval(3500);

        expectedIntervalChanges += {3500};
        QCOMPARE(resolver.interval(), 3500);
        QCOMPARE(resolver.intervalAsDuration(), 3500ms);
        QCOMPARE(intervalChanges, expectedIntervalChanges);
    }

    void lookupHostNames()
    {
        Resolver resolver;

        QVERIFY(resolver.lookupHostNames({"alpha"}));
        QVERIFY(resolver.lookupHostNames({"alpha", "beta"}));
        QVERIFY(!resolver.lookupHostNames({"alpha", "beta"}));
        QVERIFY(!resolver.lookupHostNames({"beta"}));

        // verify search domain gets removed
        QCOMPARE(resolver.domain(), "local");
        QVERIFY(!resolver.lookupHostNames({"beta.local"}));
        QVERIFY(!resolver.lookupHostNames({"beta.local."}));
    }

    void lookupServices()
    {
        Resolver resolver;

        QVERIFY(resolver.lookupServices({"_http._tcp"}));
        QVERIFY(resolver.lookupServices({"_http._tcp", "_ipp._tcp"}));
        QVERIFY(!resolver.lookupServices({"_http._tcp", "_ipp._tcp"}));
        QVERIFY(!resolver.lookupServices({"_ipp._tcp"}));

        // verify search domain gets removed
        QCOMPARE(resolver.domain(), "local");
        QVERIFY(!resolver.lookupServices({"_ipp._tcp.local"}));
        QVERIFY(!resolver.lookupServices({"_ipp._tcp.local."}));
    }

    void testHostNameQueries()
    {
        Resolver resolver;
        QSignalSpy hostNameQueryChanges{&resolver, &Resolver::hostNameQueriesChanged};
        QList<QVariantList> expectedHostNameQueryChanges;
        QStringList expectedHostNameQueries;

        // verify initial state
        QCOMPARE(resolver.domain(), "local");
        QCOMPARE(resolver.hostNameQueries(), expectedHostNameQueries);
        QCOMPARE(hostNameQueryChanges, expectedHostNameQueryChanges);

        // verify that the first query updates the list
        expectedHostNameQueries += "alpha.local";
        expectedHostNameQueryChanges += QVariantList{expectedHostNameQueries};

        QVERIFY(resolver.lookupHostNames({"alpha"}));
        QCOMPARE(resolver.hostNameQueries(), expectedHostNameQueries);
        QCOMPARE(hostNameQueryChanges, expectedHostNameQueryChanges);

        // verify that repeating the first query doesn't update the list
        QVERIFY(!resolver.lookupHostNames({"alpha"}));
        QCOMPARE(resolver.hostNameQueries(), expectedHostNameQueries);
        QCOMPARE(hostNameQueryChanges, expectedHostNameQueryChanges);

        // verify that a second query also updates the list
        expectedHostNameQueries += "beta.local";
        expectedHostNameQueryChanges += QVariantList{expectedHostNameQueries};

        QVERIFY(resolver.lookupHostNames({"alpha", "beta"}));
        QCOMPARE(resolver.hostNameQueries(), expectedHostNameQueries);
        QCOMPARE(hostNameQueryChanges, expectedHostNameQueryChanges);

        // verify that repeating the second query doesn't update the list
        QVERIFY(!resolver.lookupHostNames({"alpha", "beta"}));
        QCOMPARE(resolver.hostNameQueries(), expectedHostNameQueries);
        QCOMPARE(hostNameQueryChanges, expectedHostNameQueryChanges);

        // verify that repeating part of the second query doesn't update the list
        QVERIFY(!resolver.lookupHostNames({"beta"}));
        QCOMPARE(resolver.hostNameQueries(), expectedHostNameQueries);
        QCOMPARE(hostNameQueryChanges, expectedHostNameQueryChanges);

        // verify a raw A-record query also updates the list
        expectedHostNameQueries += "gamma.local";
        expectedHostNameQueryChanges += QVariantList{expectedHostNameQueries};

        QVERIFY(resolver.lookup(Message{}.addQuestion({"gamma.local", Message::A})));
        QCOMPARE(resolver.hostNameQueries(), expectedHostNameQueries);
        QCOMPARE(hostNameQueryChanges, expectedHostNameQueryChanges);

        // verify a raw AAAA-record query also updates the list
        expectedHostNameQueries += "delta.local";
        expectedHostNameQueryChanges += QVariantList{expectedHostNameQueries};

        QVERIFY(resolver.lookup(Message{}.addQuestion({"delta.local", Message::AAAA})));
        QCOMPARE(resolver.hostNameQueries(), expectedHostNameQueries);
        QCOMPARE(hostNameQueryChanges, expectedHostNameQueryChanges);

        // verify a repeated AAAA-record doesn't update the list
        expectedHostNameQueries += "epsilon.local";
        expectedHostNameQueryChanges += QVariantList{expectedHostNameQueries};

        QVERIFY(resolver.lookup(Message{}.addQuestion({"delta.local", Message::AAAA}).
                                addQuestion({"epsilon.local", Message::AAAA})));
        QCOMPARE(resolver.hostNameQueries(), expectedHostNameQueries);
        QCOMPARE(hostNameQueryChanges, expectedHostNameQueryChanges);
    }

    void testServiceQueries()
    {
        Resolver resolver;
        QSignalSpy serviceQueryChanges{&resolver, &Resolver::serviceQueriesChanged};
        QList<QVariantList> expectedServiceQueryChanges;
        QStringList expectedServiceQueries;

        // verify initial state
        QCOMPARE(resolver.domain(), "local");
        QCOMPARE(resolver.serviceQueries(), expectedServiceQueries);
        QCOMPARE(serviceQueryChanges, expectedServiceQueryChanges);

        // verify that the first query updates the list
        expectedServiceQueries += "_http._tcp.local";
        expectedServiceQueryChanges += QVariantList{expectedServiceQueries};

        QVERIFY(resolver.lookupServices({"_http._tcp"}));
        QCOMPARE(resolver.serviceQueries(), expectedServiceQueries);
        QCOMPARE(serviceQueryChanges, expectedServiceQueryChanges);

        // verify that repeating the first query doesn't update the list
        QVERIFY(!resolver.lookupServices({"_http._tcp"}));
        QCOMPARE(resolver.serviceQueries(), expectedServiceQueries);
        QCOMPARE(serviceQueryChanges, expectedServiceQueryChanges);

        // verify that a second query also updates the list
        expectedServiceQueries += "_ipp._tcp.local";
        expectedServiceQueryChanges += QVariantList{expectedServiceQueries};

        QVERIFY(resolver.lookupServices({"_http._tcp", "_ipp._tcp"}));
        QCOMPARE(resolver.serviceQueries(), expectedServiceQueries);
        QCOMPARE(serviceQueryChanges, expectedServiceQueryChanges);

        // verify that repeating the second query doesn't update the list
        QVERIFY(!resolver.lookupServices({"_http._tcp", "_ipp._tcp"}));
        QCOMPARE(resolver.serviceQueries(), expectedServiceQueries);
        QCOMPARE(serviceQueryChanges, expectedServiceQueryChanges);

        // verify that repeating part of the second query doesn't update the list
        QVERIFY(!resolver.lookupServices({"_ipp._tcp"}));
        QCOMPARE(resolver.serviceQueries(), expectedServiceQueries);
        QCOMPARE(serviceQueryChanges, expectedServiceQueryChanges);

        // verify a service query also updates the list
        expectedServiceQueries += "_googlecast._tcp.local";
        expectedServiceQueryChanges += QVariantList{expectedServiceQueries};

        QVERIFY(resolver.lookup(Message{}.addQuestion({"_googlecast._tcp.local", Message::PTR})));
        QCOMPARE(resolver.serviceQueries(), expectedServiceQueries);
        QCOMPARE(serviceQueryChanges, expectedServiceQueryChanges);

        // verify a repeated service query doesn't update the list
        QVERIFY(!resolver.lookup(Message{}.addQuestion({"_googlecast._tcp.local", Message::PTR})));
        QCOMPARE(resolver.serviceQueries(), expectedServiceQueries);
        QCOMPARE(serviceQueryChanges, expectedServiceQueryChanges);

        // verify an IP lookup doesn't update the list
        QVERIFY(resolver.lookup(Message{}.addQuestion({QHostAddress::LocalHost, Message::PTR})));
        QCOMPARE(resolver.serviceQueries(), expectedServiceQueries);
        QCOMPARE(serviceQueryChanges, expectedServiceQueryChanges);
    }
};

} // namespace Tests
} // namespace MDNS

QTEST_GUILESS_MAIN(MDNS::Tests::ResolverTest)

#include "tst_mdnsresolver.moc"
