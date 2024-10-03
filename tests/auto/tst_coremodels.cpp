/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */

#include "detailmodel.h"
#include "literals.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>

namespace qnc::core::tests {
namespace {

QList<int> makePath(int row, const QModelIndex &index); // -------------------------------------------------- tree paths

QList<int> makePath(const QModelIndex &index)
{
    if (index.isValid())
        return makePath(index.row(), index.parent());

    return {};
}

QList<int> makePath(int row, const QModelIndex &parent)
{
    auto path = QList<int>{};

    if (parent.isValid())
        path = makePath(parent);

    path += row;
    return path;
}

using RowList = DetailModel::RowList; // ---------------------------------------------------------- datasets for testing

RowList empty()
{
    return {};
}

RowList flat()
{
    return {
        {u"number"_s, 1},
        {u"string"_s, u"test"_s}
    };
}

RowList one()
{
    return {
        {u"one:flat"_s, QVariant::fromValue(flat())},
    };
}

RowList two()
{
    return {
        {u"two:flat"_s, QVariant::fromValue(flat())},
        {u"two:one"_s,  QVariant::fromValue( one())},
    };
}

RowList three()
{
    return {
        {u"three:flat"_s, QVariant::fromValue(flat())},
        {u"three:one"_s,  QVariant::fromValue( one())},
        {u"three:two"_s,  QVariant::fromValue( two())},
    };
}

RowList four()
{
    return {
        {u"root:flat"_s,  QVariant::fromValue( flat())},
        {u"root:one"_s,   QVariant::fromValue(  one())},
        {u"root:two"_s,   QVariant::fromValue(  two())},
        {u"root:three"_s, QVariant::fromValue(three())},
    };
}

// ----------------------------------------------------------------------------------------------------------- utilities

template <typename T>
QList<T> flatten(const QList<T> &input)
{
    auto result = QList<T>{};

    for (const auto &item : input) {
        result += item;

        if (item.hasChildren())
            result += flatten(item.children());
    }

    return result;
}


int typeId(const QVariant &variant)
{
#if QT_VERSION_MAJOR < 6
    return variant.userType();
#else
    return variant.typeId();
#endif
}

class CoreModelTest : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void testDetailModel_data()
    {
        QTest::addColumn<RowList>       ("rows");
        QTest::addColumn<QByteArrayList>("expectedWarnings");

        QTest::newRow("empty") << empty() << QByteArrayList{};
        QTest::newRow("flat")  <<  flat() << QByteArrayList{};
        QTest::newRow("tree")  <<  four() << QByteArrayList{"Maximum tree depth reached; ignoring children of (3/2/1/0)"};
    }

    void testDetailModel()
    {
        const QFETCH(DetailModel::RowList,       rows);
        const QFETCH(QByteArrayList, expectedWarnings);

#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
        QTest::failOnWarning("Index QModelIndex(-1,-1,0x0,QObject(0x0)) is not valid (expected valid)");
#endif

        auto        model = DetailModel{};
        const auto tester = QAbstractItemModelTester{&model};
        auto   modelReset = QSignalSpy{&model, &DetailModel::modelReset};

        for (const auto &warning : expectedWarnings)
            QTest::ignoreMessage(QtWarningMsg, warning.constData());

        QCOMPARE(DetailModel::validate(rows), expectedWarnings.isEmpty());

        compareDetailModel(model, {}, {});

        if (QTest::currentTestFailed())
            return;

        QCOMPARE(typeId(model.headerData(0, Qt::Horizontal)), QMetaType::QString);
        QCOMPARE(typeId(model.headerData(1, Qt::Horizontal)), QMetaType::QString);

        QVERIFY(!model.headerData(0, Qt::Horizontal).toString().isEmpty());
        QVERIFY(!model.headerData(1, Qt::Horizontal).toString().isEmpty());

        QCOMPARE(modelReset.count(), 0);

        for (const auto &warning : expectedWarnings)
            QTest::ignoreMessage(QtWarningMsg, warning.constData());

        model.reset(rows);

        if (QTest::currentTestFailed())
            return;

        QCOMPARE(modelReset.count(), 1);

        compareDetailModel(model, {}, rows);

        if (QTest::currentTestFailed())
            return;
    }

private:
    void compareDetailModel(const DetailModel &model, const QModelIndex &parent,
                            const DetailModel::RowList &expectedRows)
    {
        const auto &path            = makePath(parent);
        const auto expectedRowCount = path.length() < 4 ? expectedRows.size() : 0;

        const auto annotate = [path](const auto &value) {
            return std::make_tuple(path, value);
        };

        QCOMPARE(annotate(model.   rowCount(parent)), annotate(expectedRowCount));
        QCOMPARE(annotate(model.columnCount(parent)), annotate(2));

        for (auto row = 0; row < expectedRows.size(); ++row) {
            const auto expectValidRow = row < expectedRowCount;

            const auto &nameIndex     = model.index(row, DetailModel::Column::Name, parent);
            const auto &nameDisplay   = model.data(nameIndex, DetailModel::Role::Display);
            const auto &nameValue     = model.data(nameIndex, DetailModel::Role::Value);

            const auto &valueIndex    = model.index(row, DetailModel::Column::Value, parent);
            const auto &valueDisplay  = model.data(valueIndex, DetailModel::Role::Display);
            const auto &valueValue    = model.data(valueIndex, DetailModel::Role::Value);

            const auto &expectedName  = expectValidRow ? expectedRows[row].name  : QVariant{};
            const auto &expectedValue = expectValidRow ? expectedRows[row].value : QVariant{};

            const auto expectedStringType   = expectValidRow ? QMetaType::QString : QMetaType::UnknownType;
            const auto expectedValueDisplay = expectValidRow ? expectedValue.toString() : QVariant{};

            const auto annotate = [path = makePath(row, parent)](const auto &value) {
                return std::make_tuple(path, value);
            };

            using std::make_tuple;

            QCOMPARE(annotate(nameIndex.isValid()),     annotate(expectValidRow));
            QCOMPARE(annotate(valueIndex.isValid()),    annotate(expectValidRow));

            QCOMPARE(annotate(typeId(nameDisplay)),     annotate(expectedStringType));
            QCOMPARE(annotate(typeId(valueDisplay)),    annotate(expectedStringType));

            QCOMPARE(annotate(typeId(nameValue)),       annotate(expectedStringType));
            QCOMPARE(annotate(typeId(valueValue)),      annotate(typeId(expectedValue)));

            QCOMPARE(annotate(nameDisplay),             annotate(expectedName));
            QCOMPARE(annotate(valueDisplay),            annotate(expectedValueDisplay));

            QCOMPARE(annotate(nameValue),               annotate(expectedName));
            QCOMPARE(annotate(valueValue),              annotate(expectedValue));

            if (expectedRows[row].hasChildren()) {
                const auto actualChildren   = qvariant_cast<DetailModel::RowList>(valueValue);
                const auto expectedChildren = qvariant_cast<DetailModel::RowList>(expectedValue);

                QCOMPARE(actualChildren, expectedChildren);

                compareDetailModel(model, nameIndex, expectedChildren);

                if (QTest::currentTestFailed())
                    return;
            }
        }
    }
};

} // namespace
} // namespace qnc::mdns::tests

QTEST_GUILESS_MAIN(qnc::core::tests::CoreModelTest)

#include "tst_coremodels.moc"
