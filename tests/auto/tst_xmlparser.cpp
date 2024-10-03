/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */

// QtNetworkCrumbs headers
#include "literals.h"
#include "qnctestsupport.h"
#include "xmlparser.h"

// Qt headers
#include <QRegularExpression>
#include <QTest>
#include <QVersionNumber>

namespace qnc::xml::tests {

Q_NAMESPACE

namespace {

Q_LOGGING_CATEGORY(lcTest, "qnc.xml.tests", QtInfoMsg)

#if QT_VERSION_MAJOR < 6
using xml::qHash;
#endif // QT_VERSION_MAJOR < 6

enum Option // explicitly not class to test corner cases from implicit boolean conversion
{
    A = (1 << 0),
    B = (1 << 1),
    C = (1 << 2),
    D = (1 << 3),
    E = (1 << 4),
};

QT_WARNING_PUSH
QT_WARNING_DISABLE_CLANG("-Wunused-function")

Q_DECLARE_FLAGS(Options, Option)
Q_DECLARE_OPERATORS_FOR_FLAGS(Options)

QT_WARNING_POP

enum Direction
{
    Input  = 1,
    Output = 2,
};

enum class KnownTypes
{
    KnownType,
};

Q_ENUM_NS(KnownTypes)

using DataType = OpportunisticEnum<KnownTypes>;

struct TestResult
{
    struct Icon
    {
        QString     id        = {};
        QString     mimeType  = {};
        QSize       size      = {};
        QUrl        url       = {};
        QString     urlId     = {};
        QStringList topics    = {};
        Options     options   = {};
        Direction   direction = {};
        DataType    type      = {};
    };

    QVersionNumber version = {};
    QList<Icon>    icons   = {};
    QList<QUrl>    urls    = {};
};

using ConversionTest = std::function<void()>;

} // namespace
} // namespace qnc::xml::tests

Q_DECLARE_METATYPE(QXmlStreamReader::Error)
Q_DECLARE_METATYPE(qnc::xml::tests::ConversionTest)
Q_DECLARE_METATYPE(qnc::xml::tests::TestResult)

namespace qnc::xml {

template <>
tests::TestResult::Icon &
currentObject(tests::TestResult &result) { return result.icons.last(); }

template <>
constexpr std::size_t keyCount<tests::Direction> = 2;

template <>
constexpr KeyValueMap<tests::Direction>
keyValueMap<tests::Direction>()
{
    return {
        {
            {tests::Direction::Input,  u"in"},
            {tests::Direction::Output, u"out"},
        }
    };
}

namespace tests {
namespace {

class ParserTest : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Document,
        Root,
        Version,
        IconList,
        Icon,
    };

    Q_ENUM(State)

    enum class MinimalState {
        Document,
        Root,
    };

    Q_ENUM(MinimalState)

    using QObject::QObject;

private slots:
    void testParser_data()
    {
        const auto validXml = R"(
<?xml version="1.0"?>
<root>
  <version>
    <major>1</major>
    <minor>2</minor>
  </version>

  <icons>
    <icon id="icon-a">
      <mimetype>image/png</mimetype>
      <width>384</width>
      <height>256</height>
      <url id="url-a">/icons/test.png</url>
      <option1/>
      <option2>false</option2>
      <option3>yes</option3>
      <option4>OFF</option4>
      <option5>1</option5>
      <direction>in</direction>
      <type>KnownType</type>
    </icon>

    <icon id="icon-b">
      <mimetype>image/webp</mimetype>
      <width>768</width>
      <height>512</height>
      <url id="url-b">/icons/test.webp</url>
      <topic>test</topic>
      <option2>true</option2>
      <option3>no</option3>
      <option4>on</option4>
      <option5>0</option5>
      <direction>out</direction>
      <type>UnknownType</type>
    </icon>
  </icons>

  <url>https://ecosia.org/</url>
  <url>https://mission-lifeline.de/en/</url>
</root>)"_ba.trimmed();

        const auto namespaceXml = QByteArray{validXml}.replace("<root>", R"(<root xmlns="urn:test">)");

        const auto validResult = TestResult {
            QVersionNumber{1, 2}, {
                {
                    "icon-a"_L1, "image/png"_L1,  {384, 256},
                    "/icons/test.png"_url,  "url-a"_L1,
                    {}, Option::A | Option::C | Option::E,
                    Input, KnownTypes::KnownType,
                }, {
                    "icon-b"_L1, "image/webp"_L1, {768, 512},
                    "/icons/test.webp"_url, "url-b"_L1,
                    {"test"_L1}, Option::B | Option::D,
                    Output, "UnknownType"_L1,
                },
            }, {
                "https://ecosia.org/"_url,
                "https://mission-lifeline.de/en/"_url,
            }
        };

        const auto emptyDocumentError = QXmlStreamReader::PrematureEndOfDocumentError;
        const auto noError            = QXmlStreamReader::NoError;

        QTest::addColumn<QByteArray>             ("xml");
        QTest::addColumn<QString>                ("xmlNamespace");
        QTest::addColumn<QXmlStreamReader::Error>("expectedError");
        QTest::addColumn<TestResult>             ("expectedResult");

        QTest::newRow("empty")     << ""_ba         << ""         << emptyDocumentError << TestResult{};
        QTest::newRow("valid")     << validXml      << ""         << noError            << validResult;
        QTest::newRow("namespace") << namespaceXml  << "urn:test" << noError            << validResult;
    }

    void testParser()
    {
        const QFETCH(QByteArray,              xml);
        const QFETCH(QString,                 xmlNamespace);
        const QFETCH(QXmlStreamReader::Error, expectedError);
        const QFETCH(TestResult,              expectedResult);

        auto reader = QXmlStreamReader{xml};
        auto parser = Parser<State>{&reader};
        auto result = TestResult{};

        const Parser<State>::StateTable states = {
            {
                State::Document, {
                    {u"root",       parser.transition<State::Root>()},
                }
            }, {
                State::Root, {
                    {u"version",    parser.transition<State::Version>()},
                    {u"icons",      parser.transition<State::IconList>()},
                    {u"url",        parser.append<&TestResult::urls>(result)},
                }
            }, {
                State::Version, {
                    {u"major",      parser.assign<&TestResult::version, VersionSegment::Major>(result)},
                    {u"minor",      parser.assign<&TestResult::version, VersionSegment::Minor>(result)},
                }
            }, {
                State::IconList, {
                    {u"icon",       parser.transition<State::Icon, &TestResult::icons>(result)},
                }
            }, {
                State::Icon, {
                    {u"@id",        parser.assign<&TestResult::Icon::id>(result)},
                    {u"mimetype",   parser.assign<&TestResult::Icon::mimeType>(result)},
                    {u"width",      parser.assign<&TestResult::Icon::size, &QSize::setWidth>(result)},
                    {u"height",     parser.assign<&TestResult::Icon::size, &QSize::setHeight>(result)},
                    {u"url/@id",    parser.assign<&TestResult::Icon::urlId>(result)},
                    {u"url",        parser.assign<&TestResult::Icon::url>(result)},
                    {u"topic",      parser.append<&TestResult::Icon::topics>(result)},
                    {u"option1",    parser.assign<&TestResult::Icon::options, Option::A>(result)},
                    {u"option2",    parser.assign<&TestResult::Icon::options, Option::B>(result)},
                    {u"option3",    parser.assign<&TestResult::Icon::options, Option::C>(result)},
                    {u"option4",    parser.assign<&TestResult::Icon::options, Option::D>(result)},
                    {u"option5",    parser.assign<&TestResult::Icon::options, Option::E>(result)},
                    {u"direction",  parser.assign<&TestResult::Icon::direction>(result)},
                    {u"type",       parser.assign<&TestResult::Icon::type>(result)},
                }
            }
        };

        if (expectedError != QXmlStreamReader::NoError)
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression{R"(Error at line \d+, column \d+:)"_L1});

        const auto expectedSuccess = (expectedError == QXmlStreamReader::NoError);
        const auto success = parser.parse(lcTest(), State::Document, {{xmlNamespace, states}});

        QCOMPARE(success,                   expectedSuccess);
        QCOMPARE(reader.error(),            expectedError);
        QCOMPARE(result.version,            expectedResult.version);
        QCOMPARE(result.icons.count(),      expectedResult.icons.count());
        QCOMPARE(result.urls,               expectedResult.urls);

        for (auto i = 0; i < expectedResult.icons.count(); ++i) {
            QCOMPARE(std::make_pair(i,         result.icons[i].id),
                     std::make_pair(i, expectedResult.icons[i].id));
            QCOMPARE(std::make_pair(i,         result.icons[i].mimeType),
                     std::make_pair(i, expectedResult.icons[i].mimeType));
            QCOMPARE(std::make_pair(i,         result.icons[i].size),
                     std::make_pair(i, expectedResult.icons[i].size));
            QCOMPARE(std::make_pair(i,         result.icons[i].url),
                     std::make_pair(i, expectedResult.icons[i].url));
            QCOMPARE(std::make_pair(i,         result.icons[i].urlId),
                     std::make_pair(i, expectedResult.icons[i].urlId));
            QCOMPARE(std::make_pair(i,         result.icons[i].topics),
                     std::make_pair(i, expectedResult.icons[i].topics));
            QCOMPARE(std::make_pair(i,         result.icons[i].options),
                     std::make_pair(i, expectedResult.icons[i].options));
            QCOMPARE(std::make_pair(i,         result.icons[i].direction),
                     std::make_pair(i, expectedResult.icons[i].direction));
            QCOMPARE(std::make_pair(i,         result.icons[i].type),
                     std::make_pair(i, expectedResult.icons[i].type));
        }
    }

    void testConversions_data()
    {
        QTest::addColumn<ConversionTest>("testFunction");

        QTest::newRow("bool:invalid")       << makeConversionTest<bool>       (u"nonsense", {});
        QTest::newRow("qint8:invalid")      << makeConversionTest<qint8>      (u"nonsense", {});
        QTest::newRow("quint8:invalid")     << makeConversionTest<quint8>     (u"nonsense", {});
        QTest::newRow("short:invalid")      << makeConversionTest<short>      (u"nonsense", {});
        QTest::newRow("ushort:invalid")     << makeConversionTest<ushort>     (u"nonsense", {});
        QTest::newRow("int:invalid")        << makeConversionTest<int>        (u"nonsense", {});
        QTest::newRow("uint:invalid")       << makeConversionTest<uint>       (u"nonsense", {});
        QTest::newRow("long:invalid")       << makeConversionTest<long>       (u"nonsense", {});
        QTest::newRow("ulong:invalid")      << makeConversionTest<ulong>      (u"nonsense", {});
        QTest::newRow("qlonglong:invalid")  << makeConversionTest<qlonglong>  (u"nonsense", {});
        QTest::newRow("qulonglong:invalid") << makeConversionTest<qulonglong> (u"nonsense", {});
        QTest::newRow("longdouble:invalid") << makeConversionTest<long double>(u"nonsense", {});
        QTest::newRow("double:invalid")     << makeConversionTest<double>     (u"nonsense", {});
        QTest::newRow("float:invalid")      << makeConversionTest<float>      (u"nonsense", {});

        QTest::newRow("bool:valid")        << makeConversionTest<bool>              (u"true", true);
        QTest::newRow("qint8:valid")       << makeConversionTest<qint8>               (u"-8", -8);
        QTest::newRow("quint8:valid")      << makeConversionTest<quint8>               (u"8",  8);
        QTest::newRow("short:valid")       << makeConversionTest<short>            (u"-2048", -2048);
        QTest::newRow("ushort:valid")      << makeConversionTest<ushort>            (u"2048",  2048);
        QTest::newRow("int:valid")         << makeConversionTest<int>            (u"-524288", -524288);
        QTest::newRow("uint:valid")        << makeConversionTest<uint>            (u"524288",  524288);
        QTest::newRow("long:valid")        << makeConversionTest<long>        (u"-134217728", -134217728);
        QTest::newRow("ulong:valid")       << makeConversionTest<ulong>        (u"134217728",  134217728);
        QTest::newRow("qlonglong:valid")   << makeConversionTest<qlonglong>(u"-549755813888", -549755813888);
        QTest::newRow("qulonglong:valid")  << makeConversionTest<qulonglong>(u"549755813888",  549755813888);
        QTest::newRow("float:valid")       << makeConversionTest<float>             (u"1.23",  1.23);
        QTest::newRow("double:valid")      << makeConversionTest<double>            (u"4.56",  4.56);
        QTest::newRow("longdouble:valid")  << makeConversionTest<long double>       (u"7.89",  7.89);
        QTest::newRow("QString:valid")     << makeConversionTest<QString>    (u"Hello world", u"Hello world"_s);
        QTest::newRow("QStringView:valid") << makeConversionTest<QStringView>(u"Hello world", u"Hello world");
        QTest::newRow("QUrl:valid")        << makeConversionTest<QUrl>       (u"hello:world",  "hello:world"_url);
    }

    void testConversions()
    {
        const QFETCH(ConversionTest, testFunction);
        testFunction();
    }

private:
    template <typename T>
    static ConversionTest makeConversionTest(QStringView text, const std::optional<T> &expectedValue)
    {
        return [expectedValue, text] {
            struct TestType
            {
                T value = {};

                void setValue(T newValue) { value = std::move(newValue); }
            };

            struct TestResult
            {
                T        viaFieldAssign  = {};
                TestType viaMethodAssign = {};
            };

            const auto xml = "<root><field>%1</field><method>%1</method></root>"_L1.arg(text);

            auto reader = QXmlStreamReader{xml.toUtf8()};
            auto parser = Parser<MinimalState>{&reader};
            auto result = TestResult{};

            const Parser<MinimalState>::StateTable states = {
                {
                    MinimalState::Document, {
                        {u"root", parser.transition<MinimalState::Root>()}
                    }
                }, {
                    MinimalState::Root, {
                        {u"field",  parser.assign<&TestResult::viaFieldAssign>(result)},
                        {u"method", parser.assign<&TestResult::viaMethodAssign, &TestType::setValue>(result)},
                    }
                }
            };

            if (!expectedValue.has_value()) {
                auto pattern =
                        R"(Error at line 1, column \d+: Invalid number: %1)"_L1.
                        arg(QRegularExpression::escape(text));

                QTest::ignoreMessage(QtWarningMsg, QRegularExpression{pattern});
            }

            const auto succeeded = parser.parse(lcTest(), MinimalState::Document, states);

            QCOMPARE(succeeded, expectedValue.has_value());

            if (succeeded) {
                QCOMPARE(result.viaMethodAssign.value, expectedValue.value());
                QCOMPARE(reader.error(), QXmlStreamReader::NoError);
            } else {
                QCOMPARE(result.viaFieldAssign, T{});
                QCOMPARE(result.viaMethodAssign.value, T{});
                QCOMPARE(reader.error(), QXmlStreamReader::CustomError);
            }
        };
    }
};

} // namespace
} // namespace tests
} // namespace qnc::xml

QTEST_GUILESS_MAIN(qnc::xml::tests::ParserTest)

#include "tst_xmlparser.moc"
