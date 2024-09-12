/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2023 Mathias Hasselmann
 */

#include "qncparse.h"

#include <QTest>

Q_DECLARE_METATYPE(std::optional<short>)
Q_DECLARE_METATYPE(std::optional<ushort>)
Q_DECLARE_METATYPE(std::optional<int>)
Q_DECLARE_METATYPE(std::optional<uint>)
Q_DECLARE_METATYPE(std::optional<long>)
Q_DECLARE_METATYPE(std::optional<ulong>)
Q_DECLARE_METATYPE(std::optional<qlonglong>)
Q_DECLARE_METATYPE(std::optional<qulonglong>)
Q_DECLARE_METATYPE(std::optional<float>)
Q_DECLARE_METATYPE(std::optional<double>)

namespace qnc::core::tests {
namespace {

struct NumberTypeInfo
{
    QVariant (* parse)        (QStringView) = nullptr;
    QVariant (* parseWithBase)(QStringView, int base) = nullptr;
    bool     (* hasValue)     (const QVariant &boxedOptional) = nullptr;
    QVariant (* value)        (const QVariant &boxedOptional) = nullptr;

    bool hasSign = false;
    bool isFloat = false;
};

template <typename T>
NumberTypeInfo makeNumberTypeInfo()
{
    static constexpr auto unbox = [](const QVariant &boxedOptional) {
        return qvariant_cast<std::optional<T>>(boxedOptional);
    };

    auto info = NumberTypeInfo{};

    info.hasSign = std::is_signed_v<T>;
    info.isFloat = std::is_floating_point_v<T>;

    info.parse = [](QStringView text) {
        return QVariant::fromValue(parse<T>(text));
    };

    info.hasValue = [](const QVariant &boxedOptional) {
        return unbox(boxedOptional).has_value();
    };

    info.value = [](const QVariant &boxedOptional) {
        if (const auto optional = unbox(boxedOptional))
            return QVariant::fromValue(optional.value());
        else
            return QVariant{};
    };

    if constexpr (std::is_integral_v<T>) {
        info.parseWithBase = [](QStringView text, int base) {
            return QVariant::fromValue(parse<T>(text, base));
        };
    }

    return info;
};

} // namespace
} // namespace qnc::core::tests

Q_DECLARE_METATYPE(qnc::core::tests::NumberTypeInfo)

namespace qnc::core::tests {

class ParseTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void testParseNumbers_data()
    {
        QTest::addColumn<NumberTypeInfo>("number");

        QTest::newRow("short")      << makeNumberTypeInfo<short>();
        QTest::newRow("ushort")     << makeNumberTypeInfo<ushort>();
        QTest::newRow("int")        << makeNumberTypeInfo<int>();
        QTest::newRow("uint")       << makeNumberTypeInfo<uint>();
        QTest::newRow("long")       << makeNumberTypeInfo<long>();
        QTest::newRow("ulong")      << makeNumberTypeInfo<ulong>();
        QTest::newRow("qlonglong")  << makeNumberTypeInfo<qlonglong>();
        QTest::newRow("qulonglong") << makeNumberTypeInfo<qulonglong>();
        QTest::newRow("float")      << makeNumberTypeInfo<float>();
        QTest::newRow("double")     << makeNumberTypeInfo<double>();
    }

    void testParseNumbers()
    {
        const QFETCH(NumberTypeInfo, number);

        QCOMPARE(number.hasValue(number.parse(u"ABC")),          false);
        QCOMPARE(number.hasValue(number.parse(u"+10")),           true);
        QCOMPARE(number.hasValue(number.parse(u"-10")), number.hasSign);

        QCOMPARE(number.value(number.parse(u"+10")), +10);

        if (number.hasSign)
            QCOMPARE(number.value(number.parse(u"-10")), -10);
        if (number.parseWithBase)
            QCOMPARE(number.value(number.parseWithBase(u"21", 16)), 33);

        if (number.isFloat) {
            QCOMPARE(number.value(number.parse(u"1.2")).toFloat(), 1.2f);
            QCOMPARE(number.value(number.parse(u"-5e-3")).toFloat(), -5e-3f);
        }
    }
};

} // namespace qnc::core::tests

QTEST_GUILESS_MAIN(qnc::core::tests::ParseTest)

#include "tst_qncparse.moc"
