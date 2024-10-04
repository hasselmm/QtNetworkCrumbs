/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#ifndef QNCCORE_TREEMODEL_H
#define QNCCORE_TREEMODEL_H

// Qt headers
#include <QAbstractItemModel>

// STL headers
#include <memory>

namespace qnc::core {

class TreeModel : public QAbstractItemModel // --------------------------------------------------------------- TreeModel
{
    Q_OBJECT

public:
    enum class Role {
        Display = Qt::DisplayRole,
        Value   = Qt::UserRole + 1024,
    };

    Q_ENUM(Role)

    explicit TreeModel(QObject *parent = nullptr);

public: // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

protected:
    class Node;
    class RootNode;

    template <typename ValueType, typename BaseType = Node>
    class ValueNode;

    [[nodiscard]] Node *nodeForIndex(const QModelIndex &index);
    [[nodiscard]] const Node *nodeForIndex(const QModelIndex &index) const;
    [[nodiscard]] QModelIndex indexForNode(const Node *node) const;
    [[nodiscard]] RootNode *root() const;

#if QT_VERSION_MAJOR < 6
    inline QModelIndex createIndex(int row, int column, const Node *node) const
    { return QAbstractItemModel::createIndex(row, column, const_cast<Node *>(node)); }
#endif

private:
    RootNode *const m_root;
};

class TreeModel::Node // ------------------------------------------------------------------------------- TreeModel::Node
{
public:
    using Pointer = std::unique_ptr<Node>;

    explicit Node(const Node *parent);
    virtual ~Node() = default;

    [[nodiscard]] virtual Qt::ItemFlags flags() const { return Qt::ItemIsEnabled | Qt::ItemIsSelectable; }
    [[nodiscard]] virtual QVariant data(Role role) const;

    [[nodiscard]] virtual QVariant displayText() const { return {}; }
    [[nodiscard]] virtual QVariant value() const { return {}; }
    [[nodiscard]] virtual TreeModel *treeModel() const;

    [[nodiscard]] const Node *parent() const { return m_parent; }
    [[nodiscard]] const std::vector<Pointer> &children() const { return m_children; }
    [[nodiscard]] int childCount() const { return static_cast<int>(m_children.size()); }
    [[nodiscard]] const Node *child(int index) const;
    [[nodiscard]] int index() const;

    using Predicate = std::function<bool(const Pointer &)>;
    [[nodiscard]] Node *findChild(const Predicate &predicate) const;

    template<class NodeType, typename ...Args>
    NodeType *addChild(Args &&...args);

    template<class NodeType, typename ...Args>
    NodeType *updateOrAddChild(Args &&...args, const Predicate &predicate);

    template<class NodeType, auto IdField, typename ValueType>
    NodeType *updateOrAddChild(const ValueType &value);

    template<class NodeType, auto IdField, class ContainerType>
    void updateOrAddChildren(const ContainerType &container);

protected:
    template<class NodeType>
    const NodeType *parent() const { return dynamic_cast<const NodeType *>(m_parent); }

private:
    Node *addChild(Pointer child);

    const Node *const    m_parent;
    std::vector<Pointer> m_children = {};
};

class TreeModel::RootNode : public Node // --------------------------------------------------------- TreeModel::RootNode
{
public:
    explicit RootNode(TreeModel *model)
        : Node{nullptr}
        , m_model{model}
    {}

    TreeModel *treeModel() const override { return m_model; }
    Qt::ItemFlags flags() const override { return {}; }

private:
    TreeModel *const m_model;
};

template <typename ValueType, typename BaseType>
class TreeModel::ValueNode : public BaseType // --------------------------------------------------- TreeModel::ValueNode
{
public:
    explicit ValueNode(const ValueType &value, const Node *parent)
        : BaseType{parent}
        , m_value{value}
    {}

    QVariant displayText() const override { return BaseType::displayText(); }
    QVariant value() const override { return QVariant::fromValue(m_value); }

    [[nodiscard]] operator const ValueType &() const { return m_value; }
    void update(const ValueType &value);

protected:
    virtual void updateChildren() {}

    ValueType m_value;
};

// ------------------------------------------------------------------------------------------------ TreeModel::ValueNode

template <typename ValueType, typename BaseType>
inline void core::TreeModel::ValueNode<ValueType, BaseType>::update(const ValueType &value)
{
    const auto model = BaseType::treeModel();
    Q_ASSERT(model != nullptr);

    m_value = value;

    const auto &modelIndex = model->indexForNode(this);
    emit model->dataChanged(modelIndex, modelIndex);

    updateChildren();
}

// ----------------------------------------------------------------------------------------------------- TreeModel::Node

template<class NodeType, typename ...Args>
inline NodeType *TreeModel::Node::addChild(Args &&...args)
{
    auto node = std::make_unique<NodeType>(std::forward<Args>(args)..., this);
    return static_cast<NodeType *>(addChild(std::move(node)));
}

template<class NodeType, typename ...Args>
inline NodeType *TreeModel::Node::updateOrAddChild(Args &&...args, const Predicate &predicate)
{
    if (const auto child = dynamic_cast<NodeType *>(findChild(predicate))) {
        child->update(std::forward<Args>(args)...);
        return child;
    } else {
        return addChild<NodeType>(std::forward<Args>(args)...);
    }
}

template<class NodeType, auto IdField, typename ValueType>
inline NodeType *TreeModel::Node::updateOrAddChild(const ValueType &value)
{
    const auto predicate = [expectedId = value.*IdField](const Pointer &p) {
        if (const auto node = dynamic_cast<NodeType *>(p.get())) {
            const auto &actualId = static_cast<const ValueType &>(*node).*IdField;
            return expectedId == actualId;
        } else {
            return false;
        }
    };

    return updateOrAddChild<NodeType, const ValueType &>(value, predicate);
}

template<class NodeType, auto IdField, class ContainerType>
inline void TreeModel::Node::updateOrAddChildren(const ContainerType &container)
{
    for (const auto &value : container)
        updateOrAddChild<NodeType, IdField>(value);
}

} // namespace qnc::core

#endif // QNCCORE_TREEMODEL_H
