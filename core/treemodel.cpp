#include "treemodel.h"

namespace qnc::core {

// ----------------------------------------------------------------------------------------------------------- TreeModel

TreeModel::TreeModel(QObject *parent)
    : QAbstractItemModel{parent}
    , m_root{new RootNode{this}}
{
    connect(this, &TreeModel::modelAboutToBeReset, this, [this] {
        m_flags.setFlag(Flag::CurrentlyResetting, true);
    });

    connect(this, &TreeModel::modelReset, this, [this] {
        m_flags.setFlag(Flag::CurrentlyResetting, false);
    });
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (const auto node = nodeForIndex(parent))
        if (const auto child = node->child(row))
            if (column >= 0 && column <= 1)
                return createIndex(row, column, child);

    return {};
}

QModelIndex TreeModel::parent(const QModelIndex &child) const
{
    if (const auto childNode = nodeForIndex(child))
        return indexForNode(childNode->parent());

    return {};
}

int TreeModel::rowCount(const QModelIndex &parent) const
{
    if (const auto node = nodeForIndex(parent))
        return node->childCount();

    return 0;
}

int TreeModel::columnCount(const QModelIndex &parent) const
{
    if (const auto node = nodeForIndex(parent))
        return 1;

    return 0;
}

QVariant TreeModel::data(const QModelIndex &index, int role) const
{
    if (const auto node = nodeForIndex(index))
        return node->data(static_cast<Role>(role));

    return {};
}

Qt::ItemFlags TreeModel::flags(const QModelIndex &index) const
{
    if (const auto node = nodeForIndex(index))
        return node->flags();

    return {};
}

TreeModel::Node *TreeModel::nodeForIndex(const QModelIndex &index)
{
    if (!index.isValid())
        return m_root;
    else if (checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::DoNotUseParent))
        return static_cast<Node *>(index.internalPointer());
    else
        return nullptr;
}

const TreeModel::Node *TreeModel::nodeForIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return m_root;
    else if (checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::DoNotUseParent))
        return static_cast<const Node *>(index.internalPointer());
    else
        return nullptr;
}

QModelIndex TreeModel::indexForNode(const Node *node) const
{
    if (Q_UNLIKELY(node == nullptr))
        return {};
    if (Q_UNLIKELY(node == m_root))
        return {};

    return createIndex(node->index(), 0, node);
}

TreeModel::RootNode *TreeModel::root() const
{
    return m_root;
}

// ----------------------------------------------------------------------------------------------------- TreeModel::Node

TreeModel::Node::Node(const Node *parent)
    : m_parent{parent}
{}

QVariant TreeModel::Node::data(Role role) const
{
    switch (role) {
    case Role::Display:
        return displayText();
    case Role::Value:
        return value();
    }

    return {};
}

TreeModel *TreeModel::Node::treeModel() const
{
    if (m_parent)
        return m_parent->treeModel();

    return nullptr;
}

const TreeModel::Node *TreeModel::Node::child(int index) const
{
    if (index >= 0 && index < childCount())
        return m_children.at(static_cast<std::size_t>(index)).get();

    return nullptr;
}

int TreeModel::Node::index() const
{
    if (m_parent) {
        const auto first = m_parent->m_children.cbegin();
        const auto last = m_parent->m_children.end();
        const auto it = std::find_if(first, last, [this](const auto &ptr) {
            return ptr.get() == this;
        });

        if (Q_UNLIKELY(it == last))
            return -1;

        return static_cast<int>(it - first);
    }

    return 0;
}

void TreeModel::Node::clear()
{
    if (m_children.empty())
        return;

    const auto model = treeModel();
    Q_ASSERT(model != nullptr);

    const auto lastRow = static_cast<int>(m_children.size()) - 1;

    if (!model->m_flags.testFlag(Flag::CurrentlyResetting))
        model->beginRemoveRows(model->indexForNode(this), 0, lastRow);

    m_children.clear();

    if (!model->m_flags.testFlag(Flag::CurrentlyResetting))
        model->endRemoveRows();
}

TreeModel::Node *TreeModel::Node::addChild(Pointer child)
{
    const auto model = treeModel();
    Q_ASSERT(model != nullptr);

    const auto newRow = static_cast<int>(m_children.size());

    if (!model->m_flags.testFlag(Flag::CurrentlyResetting))
        model->beginInsertRows(model->indexForNode(this), newRow, newRow);

    const auto childPointer = child.get();
    m_children.emplace_back(std::move(child));

    if (!model->m_flags.testFlag(Flag::CurrentlyResetting))
        model->endInsertRows();

    return childPointer;
}

TreeModel::Node *TreeModel::Node::findChild(const std::function<bool(const Pointer &)> &predicate) const
{
    if (const auto it = std::find_if(m_children.cbegin(), m_children.cend(), predicate); it != m_children.cend())
        return it->get();

    return nullptr;
}

} // namespace qnc::core
