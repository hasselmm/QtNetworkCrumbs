/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */

// QtNetworkCrumbs headers
#include "qncliterals.h"
#include "qncparse.h"

// Qt headers
#include <QTest>

Q_DECLARE_METATYPE(std::function<void()>)

namespace qnc::core::tests {
namespace {

template <typename T> constexpr bool hasNegativeNumbers = std::is_signed_v<T> || std::is_same_v<T, bool>;
template <typename T> constexpr bool hasCustomBase      = std::is_integral_v<T> && !std::is_same_v<T, bool>;

template <typename T> constexpr QStringView maximumText;
template <>           constexpr QStringView maximumText     <qint16> = u"32767";
template <>           constexpr QStringView maximumText     <qint32> = u"2147483647";
template <>           constexpr QStringView maximumText     <qint64> = u"9223372036854775807";
template <>           constexpr QStringView maximumText    <quint16> = u"65535";
template <>           constexpr QStringView maximumText    <quint32> = u"4294967295";
template <>           constexpr QStringView maximumText    <quint64> = u"18446744073709551615";
template <>           constexpr QStringView maximumText      <float> = u"3.40282e+38";
template <>           constexpr QStringView maximumText     <double> = u"1.7976931348623e+308";

template <typename T> constexpr QStringView minimumText;
template <>           constexpr QStringView minimumText     <qint16> = u"-32768";
template <>           constexpr QStringView minimumText     <qint32> = u"-2147483648";
template <>           constexpr QStringView minimumText     <qint64> = u"-9223372036854775808";
template <>           constexpr QStringView minimumText    <quint16> = u"0";
template <>           constexpr QStringView minimumText    <quint32> = u"0";
template <>           constexpr QStringView minimumText    <quint64> = u"0";
template <>           constexpr QStringView minimumText      <float> = u"-3.40282e+38";
template <>           constexpr QStringView minimumText     <double> = u"-1.7976931348623e+308";

template <typename T, typename A, typename B>
constexpr QStringView select(QStringView a, QStringView b)
{
    if constexpr (sizeof(T) == sizeof(A)) {
        return a;
    } else {
        static_assert(sizeof(T) == sizeof(B));
        return b;
    }
}

template <typename T, typename A, typename B>
constexpr QStringView selectMinimumText() { return select<T, A, B>(minimumText<A>, minimumText<B>); }
template <typename T, typename A, typename B>
constexpr QStringView selectMaximumText() { return select<T, A, B>(maximumText<A>, maximumText<B>); }

template <> constexpr QStringView maximumText<long>  = selectMaximumText<long,  qint32,  qint64>();
template <> constexpr QStringView minimumText<long>  = selectMinimumText<long,  qint32,  qint64>();
template <> constexpr QStringView maximumText<ulong> = selectMaximumText<ulong, quint32, quint64>();
template <> constexpr QStringView minimumText<ulong> = selectMinimumText<ulong, quint32, quint64>();

QString incrementLastChar(QStringView text)
{
    if (text == u"0")
        return u"-1"_s;

    auto result     = text.toString();
    const auto back = result.end() - 1;

    if (!back->isDigit())
        return {};

    *back = QChar{back->unicode() + 1};

    return result;
}

template <typename T>
std::function<void()> makeTestParseNumbers()
{
    return [] {
        QVERIFY(!maximumText<T>.isEmpty());
        QVERIFY(!minimumText<T>.isEmpty());

        auto aboveMaximumText = incrementLastChar(maximumText<T>); // max. numbers for all int types end with 7 or 5
        auto belowMinimumText = incrementLastChar(minimumText<T>); // min. numbers or int types all are 0 or end with 8

        const auto  invalidNumber = parse<T>(u"ABC");
        const auto positiveNumber = parse<T>(u"+10");
        const auto negativeNumber = parse<T>(u"-10");
        const auto  maximumNumber = parse<T>(maximumText<T>);
        const auto  minimumNumber = parse<T>(minimumText<T>);
        const auto    aboveNumber = parse<T>(aboveMaximumText);
        const auto    belowNumber = parse<T>(belowMinimumText);

        QVERIFY (!invalidNumber.has_value());
        QVERIFY (positiveNumber.has_value());
        QVERIFY2( maximumNumber.has_value(), qPrintable(maximumText<T>.toString()));
        QVERIFY2( minimumNumber.has_value(), qPrintable(minimumText<T>.toString()));
        QVERIFY2(!  aboveNumber.has_value(), qPrintable(aboveMaximumText));
        QVERIFY2(!  belowNumber.has_value(), qPrintable(belowMinimumText));

        QCOMPARE(positiveNumber.value(), static_cast<T>(+10));

        QCOMPARE( maximumNumber.value(), std::numeric_limits<T>::max());
        QCOMPARE( minimumNumber.value(), std::numeric_limits<T>::lowest());

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
            const auto positiveInfinity = parse<T>(u"inf");
            const auto negativeFloat    = parse<T>(u"-5e-3");
            const auto negativeInfinity = parse<T>(u"-inf");
            const auto nan              = parse<T>(u"nan");

            QVERIFY (positiveFloat.has_value());
            QCOMPARE(positiveFloat.value(), static_cast<T>(1.23));

            QVERIFY (negativeFloat.has_value());
            QCOMPARE(negativeFloat.value(), static_cast<T>(-5e-3));

            QVERIFY(positiveInfinity.has_value());
            QVERIFY(std::isinf(positiveInfinity.value()));

            QVERIFY(negativeInfinity.has_value());
            QVERIFY(std::isinf(negativeInfinity.value()));

            QVERIFY(nan.has_value());
            QVERIFY(std::isnan(nan.value()));
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
