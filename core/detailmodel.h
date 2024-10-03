/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#ifndef QNCCORE_DETAILMODEL_H
#define QNCCORE_DETAILMODEL_H

// QtNetworkCrumbs headers
#include "compat.h"

// Qt headers
#include <QAbstractItemModel>

namespace qnc::core {

class DetailModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum class Column : int
    {
        Name,
        Value,
    };

    enum class Role : int
    {
        Display = Qt::DisplayRole,
        Value = Qt::UserRole + 1024,
    };

    struct Row;
    using RowList = QList<Row>;

    struct Row
    {
        QString  name;
        QVariant value;

        [[nodiscard]] QVariant data(Column column, Role role) const;
        [[nodiscard]] QVariant displayData(Column column) const;
        [[nodiscard]] QVariant valueData(Column column) const;

        [[nodiscard]] friend bool operator==(const Row &l, const Row &r)
        {
            return std::tie(l.name, l.value) == std::tie(r.name, r.value);
        }

        [[nodiscard]] bool hasChildren() const { return DetailModel::hasChildren(value); }
        [[nodiscard]] RowList children() const { return DetailModel::children(value); }
    };

    using QAbstractItemModel::QAbstractItemModel;

    void reset(const RowList &rows);

    static QVariant value(const QModelIndex &index);
    static QUrl url(const QModelIndex &index);

    QModelIndex index(int row, Column column, const QModelIndex &parent = {}) const
    {
        return index(row, qToUnderlying(column), parent);
    }

    QVariant data(const QModelIndex &index, Role role) const
    {
        return data(index, qToUnderlying(role));
    }

    [[nodiscard]] static bool validate(const RowList &rows);

public: // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

protected:
    [[nodiscard]] static bool hasChildren(const QVariant &value);
    [[nodiscard]] static RowList children(const QVariant &value) { return qvariant_cast<RowList>(value); }

private:
    RowList m_rows;
};

} // namespace qnc::core

Q_DECLARE_METATYPE(qnc::core::DetailModel::Row)
Q_DECLARE_METATYPE(qnc::core::DetailModel::RowList)

inline bool qnc::core::DetailModel::hasChildren(const QVariant &value)
{
#if QT_VERSION_MAJOR < 6
    return value.userType() == qMetaTypeId<RowList>();
#else
    return value.typeId() == qMetaTypeId<RowList>();
#endif
}

#endif // QNCCORE_DETAILMODEL_H
