/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2023 Mathias Hasselmann
 */

// QtNetworkCrumbs headers
#include "qncliterals.h"
#include "xmlparser.h"

// Qt headers
#include <QRegularExpression>
#include <QTest>
#include <QVersionNumber>

namespace qnc::xml::tests {
namespace {

Q_LOGGING_CATEGORY(lcTest, "qnc.xml.tests")

#if QT_VERSION_MAJOR < 6
using xml::qHash;
#endif // QT_VERSION_MAJOR < 6

struct TestResult
{
    struct Icon
    {
        QString mimeType = {};
        QSize   size     = {};
        QUrl    url      = {};
    };

    QVersionNumber version = {};
    QList<Icon>    icons   = {};
};

} // namespace
} // namespace qnc::xml::tests

Q_DECLARE_METATYPE(QXmlStreamReader::Error)
Q_DECLARE_METATYPE(qnc::xml::tests::TestResult)

template <>
qnc::xml::tests::TestResult::Icon &
qnc::xml::currentObject(tests::TestResult &result) { return result.icons.last(); }

namespace qnc::xml::tests {
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
    <icon>
      <mimetype>image/png</mimetype>
      <width>384</width>
      <height>256</height>
      <url>/icons/test.png</url>
    </icon>

    <icon>
      <mimetype>image/webp</mimetype>
      <width>768</width>
      <height>512</height>
      <url>/icons/test.webp</url>
    </icon>
  </icons>
</root>)"_ba.trimmed();

        const auto namespaceXml = QByteArray{validXml}.replace("<root>", R"(<root xmlns="urn:test">)");

        const auto validResult = TestResult {
            QVersionNumber{1, 2}, {
                {"image/png"_L1,  {384, 256}, "/icons/test.png"_url},
                {"image/webp"_L1, {768, 512}, "/icons/test.webp"_url},
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
                }
            }, {
                State::Version, {
                    {u"major",      parser.setField<&TestResult::version, VersionSegment::Major>(result)},
                    {u"minor",      parser.setField<&TestResult::version, VersionSegment::Minor>(result)},
                }
            }, {
                State::IconList, {
                    {u"icon",       parser.transition<State::Icon, &TestResult::icons>(result)},
                }
            }, {
                State::Icon, {
                    {u"mimetype",   parser.setField<&TestResult::Icon::mimeType>(result)},
                    {u"width",      parser.setField<&TestResult::Icon::size, &QSize::setWidth>(result)},
                    {u"height",     parser.setField<&TestResult::Icon::size, &QSize::setHeight>(result)},
                    {u"url",        parser.setField<&TestResult::Icon::url>(result)},
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

        for (auto i = 0; i < expectedResult.icons.count(); ++i) {
            QCOMPARE(std::make_pair(i,         result.icons[i].mimeType),
                     std::make_pair(i, expectedResult.icons[i].mimeType));
            QCOMPARE(std::make_pair(i,         result.icons[i].size),
                     std::make_pair(i, expectedResult.icons[i].size));
            QCOMPARE(std::make_pair(i,         result.icons[i].url),
                     std::make_pair(i, expectedResult.icons[i].url));
        }
    }
};

} // namespace
} // namespace qnc::xml::tests

QTEST_GUILESS_MAIN(qnc::xml::tests::ParserTest)

#include "tst_xmlparser.moc"
