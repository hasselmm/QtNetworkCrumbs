/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2021 Mathias Hasselmann
 */

// MDNS headers
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
};

} // namespace Tests
} // namespace MDNS

QTEST_GUILESS_MAIN(MDNS::Tests::ResolverTest)

#include "tst_mdnsresolver.moc"
