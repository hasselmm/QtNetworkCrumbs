/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */

// QtNetworkCrumbs headers
#include "qncliterals.h"
#include "upnpresolver.h"
#include "upnptreemodel.h"

// Qt headers
#include <QApplication>
#include <QDesktopServices>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMainWindow>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QTreeView>

namespace qnc::upnp::demo {
namespace {

using ssdp::ServiceLookupRequest;

template <auto Role>
[[nodiscard]] QVariant modelData(const QModelIndex &index)
{
    return index.data(static_cast<int>(Role));
}

template <auto Column, auto Role>
[[nodiscard]] QVariant modelData(const QModelIndex &index)
{
    return index.siblingAtColumn(static_cast<int>(Column)).data(static_cast<int>(Role));
}

struct Path
{
    static constexpr int      BitsPerLength = 2;
    static constexpr int      BitsPerRow    = 10;
    static constexpr int      MaximumLength = (1 << BitsPerLength) - 1;
    static constexpr int      MaximumRow    = (1 << BitsPerRow)    - 1;
    static constexpr quintptr LengthMask    = static_cast<quintptr>(MaximumLength);
    static constexpr quintptr RowMask       = static_cast<quintptr>(MaximumRow);
    static constexpr quintptr IndexMask     = ~LengthMask;

    quintptr value;

    Q_IMPLICIT constexpr Path(quintptr path = 0) noexcept : value{path} {}
    Q_IMPLICIT constexpr operator quintptr() const noexcept { return value; }

    explicit constexpr Path(Path parent, int row) noexcept : value{make(parent, row).value} {}
    explicit constexpr Path(const QModelIndex &index, int row) noexcept : Path{index.internalId(), row} {}

    [[nodiscard]] constexpr static Path make(Path parent, int row) noexcept
    {
        if (row < 0 || row > MaximumRow)
            return 0;
        if (parent.length() >= MaximumLength)
            return 0;

        const auto shift = parent.length() * BitsPerRow + BitsPerLength;

        return (parent.value & IndexMask)
                | static_cast<quintptr>(row << shift)
                | static_cast<quintptr>(parent.length() + 1);
    }

    [[nodiscard]] constexpr int length() const noexcept
    {
        return static_cast<int>(value & LengthMask);
    }

    [[nodiscard]] constexpr int at(int index) const noexcept
    {
        if (Q_UNLIKELY(index < 0 || index >= length()))
            return -1;

        const auto shift = index * BitsPerRow + BitsPerLength;
        return static_cast<int>((value >> shift) & RowMask);
    }

    [[nodiscard]] constexpr int last() const noexcept
    {
        return at(length() - 1);
    }

    [[nodiscard]] constexpr Path parent() const noexcept
    {
        if (Q_UNLIKELY(length() < 1))
            return {};

        const auto shift = (length() - 1) * BitsPerRow + BitsPerLength;
        const auto prefix = (value & ~(RowMask << shift)) & IndexMask;
        return prefix | static_cast<quintptr>(length() - 1);
    }

    [[nodiscard]] friend constexpr bool operator==(const Path &l, const Path &r) noexcept { return l.value == r.value; }
};

static_assert(Path{}    .length() == 0);
static_assert(Path{0x00}.length() == 0);
static_assert(Path{0x01}.length() == 1);
static_assert(Path{0x11}.length() == 1);
static_assert(Path{0x13}.length() == 3);

static_assert(Path{}      .at(0) == -1);
static_assert(Path{0x0011}.at(0) ==  4);
static_assert(Path{0x0012}.at(1) ==  0);

static_assert(Path{0x841602f}.length()          ==  3);
static_assert(Path{0x841602f}.at(0)             == 11);
static_assert(Path{0x841602f}.at(1)             == 22);
static_assert(Path{0x841602f}.at(2)             == 33);
static_assert(Path{0x841602f}.at(3)             == -1);

static_assert(Path{0x841602f}.parent().length() ==  2);
static_assert(Path{0x841602f}.parent().at(0)    == 11);
static_assert(Path{0x841602f}.parent().at(1)    == 22);
static_assert(Path{0x841602f}.parent().at(2)    == -1);

static_assert(Path{0x1602e}.length() ==  2);
static_assert(Path{0x1602e}.at(0)    == 11);
static_assert(Path{0x1602e}.at(1)    == 22);
static_assert(Path{0x1602e}.at(2)    == -1);

static_assert(Path{0x1602e, 33}.length() ==  3);
static_assert(Path{0x1602e, 33}.at(0)    == 11);
static_assert(Path{0x1602e, 33}.at(1)    == 22);
static_assert(Path{0x1602e, 33}.at(2)    == 33);
static_assert(Path{0x1602e, 33}.at(3)    == -1);

static_assert(Path{0x841602f}.parent() == Path{0x1602e});
static_assert(Path{0x841602f}          == Path{0x1602e, 33});

class DetailModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum class Column
    {
        Name,
        Value,
    };

    enum class Role
    {
        Display = Qt::DisplayRole,
        Value = Qt::UserRole + 1024,
    };

    struct Row
    {
        QString  name;
        QVariant value;

        [[nodiscard]] QVariant data(Column column, Role role) const;
        [[nodiscard]] QVariant displayData(Column column) const;
        [[nodiscard]] QVariant valueData(Column column) const;
    };

    using RowList = QList<Row>;

    using QAbstractItemModel::QAbstractItemModel;

    void reset(const RowList &rows);

public: // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QVariant value(const QModelIndex &index) const;

    RowList m_rows;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void onSelectionChanged(const QItemSelection &selected);
    void onDetailActivated(const QModelIndex &index);

    template <typename T>
    static DetailModel::RowList details(const T &value);

    template <typename T>
    static DetailModel::RowList details(const T &value, const QModelIndex &index);

    TreeModel   *const m_treeModel   = new TreeModel{this};
    DetailModel *const m_detailModel = new DetailModel{this};

    QSplitter *const m_splitter   = new QSplitter{this};
    QTreeView *const m_treeView   = new QTreeView{m_splitter};
    QTreeView *const m_detailView = new QTreeView{m_splitter};
};

class Viewer : public QApplication
{
    Q_OBJECT

public:
    using QApplication::QApplication;

    int run();
};

void DetailModel::reset(const RowList &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

QModelIndex DetailModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return createIndex(row, column, Path{parent, parent.row()});

    return createIndex(row, column, Path{});
}

QModelIndex DetailModel::parent(const QModelIndex &child) const
{
    if (const auto path = Path{child.internalId()}; path.length() > 0)
        return createIndex(path.last(), 0, path.parent());

    return {};
}

int DetailModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return static_cast<int>(m_rows.size());
    else if (const auto &data = value(parent); data.canConvert<RowList>())
        return static_cast<int>(qvariant_cast<RowList>(data).size());
    else
        return 0;
}

int DetailModel::columnCount(const QModelIndex &) const
{
    return 2;
}

QVariant DetailModel::Row::data(Column column, Role role) const
{
    switch (static_cast<Role>(role)) {
    case Role::Display:
        return displayData(column);
    case Role::Value:
        return valueData(column);
    }

    return {};
}

QVariant DetailModel::Row::displayData(Column column) const
{
    switch (column) {
    case Column::Name:
        return name;
    case Column::Value:
        return value.toString();
    }

    return {};
}

QVariant DetailModel::Row::valueData(Column column) const
{
    switch (column) {
    case Column::Name:
        return name;
    case Column::Value:
        return value;
    }

    return {};
}

QVariant DetailModel::data(const QModelIndex &index, int role) const
{
    if (checkIndex(index, CheckIndexOption::IndexIsValid)) {
        const auto path = Path{index.internalId()};
        auto rowList = m_rows;

        for (auto i = 0; i < path.length(); ++i) {
            const auto &row = rowList.at(path.at(i));
            Q_ASSERT(row.value.canConvert<RowList>());
            rowList = qvariant_cast<RowList>(row.value);
        }

        const auto &row = rowList.at(index.row());
        const auto column = static_cast<Column>(index.column());
        return row.data(column, static_cast<Role>(role));
    }

    return {};
}

QVariant DetailModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (static_cast<Column>(section)) {
        case Column::Name:
            return tr("Property");
        case Column::Value:
            return tr("Value");
        }
    }

    return {};
}

QVariant DetailModel::value(const QModelIndex &index) const
{
    return modelData<Column::Value, Role::Value>(index);
}

void applySizePolicy(QWidget *widget, std::function<void(QSizePolicy &)> setter)
{
    auto policy = widget->sizePolicy();
    setter(policy);
    widget->setSizePolicy(policy);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow{parent}
{
    m_treeModel->setNetworkAccessManager(new QNetworkAccessManager{this});
    m_treeModel->addQuery("ssdp:all"_L1);
    // m_treeModel->addQuery("upnp:rootdevice"_L1);
    // m_treeModel->addQuery("urn:schemas-upnp-org:device:MediaRenderer:1"_L1);
    // m_treeModel->addQuery("urn:schemas-sony-com:service:IRCC:1"_L1);
    // m_treeModel->addQuery("urn:schemas-sony-com:service:ScalarWebAPI:1"_L1);

    setCentralWidget(m_splitter);

    const auto sortedTreeModel = new QSortFilterProxyModel{this};
    sortedTreeModel->setSourceModel(m_treeModel);

    m_treeView->setHeaderHidden(true);
    m_treeView->setModel(sortedTreeModel);

    m_detailView->header()->setStretchLastSection(true);
    m_detailView->setSelectionBehavior(QTreeView::SelectRows);
    m_detailView->setAlternatingRowColors(true);
    m_detailView->setModel(m_detailModel);

    applySizePolicy(m_treeView, [](auto &policy) {
        policy.setHorizontalStretch(1);
    });

    applySizePolicy(m_detailView, [](auto &policy) {
        policy.setHorizontalStretch(2);
    });

    connect(m_detailView, &QTreeView::activated,
            this, &MainWindow::onDetailActivated);
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onSelectionChanged);
}

template <>
DetailModel::RowList MainWindow::details(const DeviceManufacturer &manufacturer)
{
    return {
        {tr("name"), manufacturer.name},
        {tr("url"),  manufacturer.url},
    };
}

template <>
DetailModel::RowList MainWindow::details(const DeviceModel &model)
{
    return {
        {tr("universalProductCode"), model.universalProductCode},
        {tr("description"),          model.description},
        {tr("name"),                 model.name},
        {tr("number"),               model.number},
        {tr("url"),                  model.url},
    };
}

template <>
DetailModel::RowList MainWindow::details(const DeviceDescription &device)
{
    return {
        {tr("url"),                                      device.url},
        {tr("specVersion"),          QVariant::fromValue(device.specVersion)},
        {tr("uniqueDeviceName"),                         device.uniqueDeviceName},
        {tr("deviceType"),                               device.deviceType},
        {tr("displayName"),                              device.displayName},
        {tr("manufacturer"), QVariant::fromValue(details(device.manufacturer))},
        {tr("model"),        QVariant::fromValue(details(device.model))},
        {tr("presentationUrl"),                          device.presentationUrl},
        {tr("serialNumber"),                             device.serialNumber},
    };
}

template <>
DetailModel::RowList MainWindow::details(const ServiceDescription &service, const QModelIndex &index)
{
    const auto &baseUrl = qvariant_cast<QUrl>(modelData<TreeModel::Role::DeviceUrl>(index));

    return {
        {tr("id"),          service.id},
        {tr("type"),        service.type},
        {tr("scpdUrl"),     baseUrl.resolved(service.scpdUrl)},
        {tr("controlUrl"),  baseUrl.resolved(service.controlUrl)},
        {tr("eventingUrl"), baseUrl.resolved(service.eventingUrl)},
    };
}

template <>
DetailModel::RowList MainWindow::details(const ArgumentDescription &argument)
{
    return {
        {tr("direction"), QVariant::fromValue(argument.direction)},
        {tr("flags"),     QVariant::fromValue(argument.flags)},
        {tr("stateVariable"),                 argument.stateVariable},
    };
}

template <>
DetailModel::RowList MainWindow::details(const QList<ArgumentDescription> &argumentList)
{
    auto rows = DetailModel::RowList{};
    rows.reserve(argumentList.size());

    for (const auto &argument : argumentList)
        rows.emplaceBack(DetailModel::Row{argument.name, QVariant::fromValue(details(argument))});

    return rows;
}

template <>
DetailModel::RowList MainWindow::details(const ActionDescription &action)
{
    return {
        {tr("name"),                          action.name},
        {tr("flags"),     QVariant::fromValue(action.flags)},
        {tr("arguments"), QVariant::fromValue(details(action.arguments))},
    };
}

template <>
DetailModel::RowList MainWindow::details(const ValueRangeDescription &valueRange)
{
    return {
        {tr("minimum"), valueRange.minimum},
        {tr("maximum"), valueRange.maximum},
        {tr("step"),    valueRange.step},
    };
}

template <>
DetailModel::RowList MainWindow::details(const StateVariableDescription &variable)
{
    return {
        {tr("name"),                                   variable.name},
        {tr("flags"),              QVariant::fromValue(variable.flags)},
        {tr("dataType"),      QVariant::fromStdVariant(variable.dataType)},
        {tr("defaultValue"),                           variable.defaultValue},
        {tr("allowedValues"),                          variable.allowedValues},
        {tr("valueRange"), QVariant::fromValue(details(variable.valueRange))},
    };
}

void MainWindow::onSelectionChanged(const QItemSelection &selected)
{
    const auto &indexes = selected.indexes();

    if (indexes.isEmpty())
        return;

    const auto &index = indexes.constFirst();
    const auto &data  = modelData<TreeModel::Role::Value>(index);

    if (data.canConvert<DeviceDescription>()) {
        m_detailModel->reset(details(qvariant_cast<DeviceDescription>(data)));
    } else if (data.canConvert<ServiceDescription>()) {
        m_detailModel->reset(details(qvariant_cast<ServiceDescription>(data), index));
    } else if (data.canConvert<ActionDescription>()) {
        m_detailModel->reset(details(qvariant_cast<ActionDescription>(data)));
    } else if (data.canConvert<StateVariableDescription>()) {
        m_detailModel->reset(details(qvariant_cast<StateVariableDescription>(data)));
    } else {
        m_detailModel->reset({});
    }

    m_detailView->expandAll();
    m_detailView->header()->resizeSections(QHeaderView::ResizeToContents);
}

void MainWindow::onDetailActivated(const QModelIndex &index)
{
    const auto &data = modelData<DetailModel::Column::Value, DetailModel::Role::Value>(index);
    if (const auto &url = qvariant_cast<QUrl>(data); !url.isEmpty())
        QDesktopServices::openUrl(url);
}

int Viewer::run()
{
    auto window = MainWindow{};
    window.show();
    return exec();
}

} // namespace
} // namespace qnc::upnp::demo

int main(int argc, char *argv[])
{
    return qnc::upnp::demo::Viewer{argc, argv}.run();
}

#include "upnpviewer.moc"
