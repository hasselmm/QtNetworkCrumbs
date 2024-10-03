/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#include "detailmodel.h"

// Qt headers
#include <QLoggingCategory>
#include <QUrl>

namespace qnc::core {
namespace {

Q_LOGGING_CATEGORY(lcDetailModel, "qnc.core.detailmodel")

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

    explicit constexpr Path(const QModelIndex &index) noexcept : Path{index.internalId()} {}
    explicit constexpr Path(Path parent, int row) noexcept : value{make(parent, row).value} {}
    explicit constexpr Path(const QModelIndex &parent, int row) noexcept : Path{parent.internalId(), row} {}

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

template <DetailModel::Column column, DetailModel::Role role>
[[nodiscard]] QVariant modelData(const QModelIndex &index)
{
    const auto &sibling = index.siblingAtColumn(qToUnderlying(column));
    return sibling.data(qToUnderlying(role));
}

QByteArray toByteArray(Path path)
{
    auto string = QByteArray{};

    if (path.length() > 0) {
        string += QByteArray::number(path.at(0));

        for (auto i = 1; i < path.length(); ++i) {
            string += '/';
            string += QByteArray::number(path.at(i));
        }
    }

    return string;
}

bool validate(const DetailModel::RowList &rows, Path path = {})
{
    auto valid = true;

    if (rows.count() > Path::MaximumRow) {
        qCWarning(lcDetailModel, "Too many items at (%s); ignoring rows from %d to %d",
                  toByteArray(path).constData(), Path::MaximumRow + 1,
                  static_cast<int>(rows.count()));

        valid = false;
    }

    for (auto i = 0; i < rows.count(); ++i) {
        if (rows[i].hasChildren()) {
            if (path.length() == Path::MaximumLength) {
                qCWarning(lcDetailModel,
                          "Maximum tree depth reached; ignoring children of (%s/%d)",
                          toByteArray(path).constData(), i);

                valid = false;
            } else {
                valid &= validate(rows[i].children(), Path{path, i});
            }
        }
    }

    return valid;
}

} // namespace

void DetailModel::reset(const RowList &rows)
{
    core::validate(rows);

    beginResetModel();
    m_rows = rows;
    endResetModel();
}

QModelIndex DetailModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!parent.isValid())
        return createIndex(row, column, Path{});
    else if (Path{parent}.length() < Path::MaximumLength)
        return createIndex(row, column, Path{parent, parent.row()});
    else
        return {};
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
        return std::min(static_cast<int>(m_rows.size()), Path::MaximumRow);
    else if (parent.column() != 0)
        return 0;
    else if (Path{parent}.length() == Path::MaximumLength)
        return 0;
    else if (const auto &data = value(parent); !hasChildren(data))
        return 0;
    else
        return std::min(static_cast<int>(children(data).size()), Path::MaximumRow);
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
    if (index.isValid() && checkIndex(index)) {
        const auto path = Path{index.internalId()};
        auto rowList = m_rows;

        for (auto i = 0; i < path.length(); ++i) {
            const auto &row = rowList.at(path.at(i));
            Q_ASSERT(row.hasChildren());
            rowList = row.children();
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

QVariant DetailModel::value(const QModelIndex &index)
{
    return modelData<Column::Value, Role::Value>(index);
}

QUrl DetailModel::url(const QModelIndex &index)
{
    if (const auto &data = value(index); data.userType() == QMetaType::QUrl)
        return data.toUrl();

    return {};
}

bool DetailModel::validate(const RowList &rows)
{
    return core::validate(rows);
}

} // namespace qnc::core
