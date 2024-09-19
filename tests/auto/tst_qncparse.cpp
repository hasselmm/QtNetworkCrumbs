/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */

// QtNetworkCrumbs headers
#include "qncparse.h"

// Qt headers
#include <QTest>

Q_DECLARE_METATYPE(std::function<void()>)

namespace qnc::core::tests {
namespace {

template <typename T> constexpr bool hasNegativeNumbers = std::is_signed_v<T> || std::is_same_v<T, bool>;
template <typename T> constexpr bool hasCustomBase      = std::is_integral_v<T> && !std::is_same_v<T, bool>;

template <typename T>
std::function<void()> makeTestParseNumbers()
{
    return [] {
        const auto  invalidNumber = parse<T>(u"ABC");
        const auto positiveNumber = parse<T>(u"+10");
        const auto negativeNumber = parse<T>(u"-10");

        QVERIFY (!invalidNumber.has_value());
        QVERIFY (positiveNumber.has_value());

        QCOMPARE(positiveNumber.value(), static_cast<T>(+10));

        if constexpr (hasNegativeNumbers<T>) {
            QVERIFY(negativeNumber.has_value());
            QCOMPARE(negativeNumber.value(), static_cast<T>(-10));
        } else {
            QVERIFY(!negativeNumber.has_value());
        }

        if constexpr (hasCustomBase<T>) {
            QCOMPARE(parse<T>(u"21",  8), 17);
            QCOMPARE(parse<T>(u"21", 10), 21);
            QCOMPARE(parse<T>(u"21", 16), 33);
        }

        if constexpr (std::is_floating_point_v<T>) {
            const auto positiveFloat    = parse<T>(u"1.23");
            const auto negativeFloat    = parse<T>(u"-5e-3");

            QVERIFY (positiveFloat.has_value());
            QCOMPARE(positiveFloat.value(), static_cast<T>(1.23));

            QVERIFY (negativeFloat.has_value());
            QCOMPARE(negativeFloat.value(), static_cast<T>(-5e-3));
        }
    };
}

} // namespace
} // namespace qnc::core::tests

namespace qnc::core::tests {

class ParseTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void testParseNumbers_data()
    {
        QTest::addColumn<std::function<void()>>("testFunction");

        QTest::newRow("short")      << makeTestParseNumbers<short>();
        QTest::newRow("ushort")     << makeTestParseNumbers<ushort>();
        QTest::newRow("int")        << makeTestParseNumbers<int>();
        QTest::newRow("uint")       << makeTestParseNumbers<uint>();
        QTest::newRow("long")       << makeTestParseNumbers<long>();
        QTest::newRow("ulong")      << makeTestParseNumbers<ulong>();
        QTest::newRow("qlonglong")  << makeTestParseNumbers<qlonglong>();
        QTest::newRow("qulonglong") << makeTestParseNumbers<qulonglong>();
        QTest::newRow("float")      << makeTestParseNumbers<float>();
        QTest::newRow("double")     << makeTestParseNumbers<double>();
    }

    void testParseNumbers()
    {
        const QFETCH(std::function<void()>, testFunction);
        testFunction();
    }
};

} // namespace qnc::core::tests

QTEST_GUILESS_MAIN(qnc::core::tests::ParseTest)

#include "tst_qncparse.moc"
