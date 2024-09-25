/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#ifndef QNCUPNP_UPNPTREEMODEL_H
#define QNCUPNP_UPNPTREEMODEL_H

// Qt headers
#include <QAbstractItemModel>
#include <QPointer>

class QNetworkAccessManager;

namespace qnc::upnp {

inline namespace scpd {
struct ActionDescription;
struct StateVariableDescription;
} // inline namespace scpd

struct DeviceDescription;
struct ServiceDescription;

class Resolver;

class TreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum class Role {
        Display = Qt::DisplayRole,
        Value   = Qt::UserRole + 1024,
        Device,
        DeviceUrl,
    };

    Q_ENUM(Role)

    explicit TreeModel(QObject *parent = nullptr);

    void addQuery(const QString &serviceName);

    void setNetworkAccessManager(QNetworkAccessManager *newManager);
    QNetworkAccessManager *networkAccessManager() const;

public: // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    struct DeviceType;

    class Node;
    class RootNode;

    template <typename ValueType> class ValueNode;

    using DeviceTypeNode = ValueNode<DeviceType>;
    using DeviceNode     = ValueNode<DeviceDescription>;
    using ServiceNode    = ValueNode<ServiceDescription>;
    using ActionNode     = ValueNode<ActionDescription>;
    using VariableNode   = ValueNode<StateVariableDescription>;

    const Node *nodeForIndex(const QModelIndex &index) const;
    QModelIndex indexForNode(const Node *node) const;

    RootNode *const m_root;
    Resolver *const m_resolver;
};

} // namespace qnc::upnp

#endif // QNCUPNP_UPNPTREEMODEL_H
