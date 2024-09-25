/* QtNetworkCrumbs - Some networking toys for Qt
 * Copyright (C) 2019-2024 Mathias Hasselmann
 */
#include "upnptreemodel.h"

// QtNetworkCrumbs headers
#include "upnpresolver.h"

// Qt headers
#include <QLoggingCategory>

namespace qnc::upnp {

namespace {

Q_LOGGING_CATEGORY(lcTreeModel, "qnc.upnp.treemodel")

template <typename T>
QVariant makeVariant(const std::optional<T> &optional)
{
    if (optional.has_value())
        return QVariant::fromValue(optional.value());

    return {};
}

} // namespace

using ssdp::ServiceLookupRequest;

// ---------------------------------------------------------------------------------------------------------------------

struct TreeModel::DeviceType
{
    QString id;
};

// ---------------------------------------------------------------------------------------------------------------------

class TreeModel::Node
{
public:
    using Pointer = std::unique_ptr<Node>;

    explicit Node(const Node *parent);
    virtual ~Node() = default;

    [[nodiscard]] virtual Qt::ItemFlags flags() const { return Qt::ItemIsEnabled | Qt::ItemIsSelectable; }
    [[nodiscard]] virtual QVariant data(Role role) const;

    [[nodiscard]] virtual QVariant displayText() const { return {}; }
    [[nodiscard]] virtual QVariant value() const { return {}; }
    [[nodiscard]] virtual std::optional<DeviceDescription> device() const;
    [[nodiscard]] virtual QUrl deviceUrl() const;
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

private:
    const Node *const    m_parent;
    std::vector<Pointer> m_children = {};
};

// ---------------------------------------------------------------------------------------------------------------------

class TreeModel::RootNode : public Node
{
public:
    explicit RootNode(TreeModel *model)
        : Node{nullptr}
        , m_model{model}
    {}

    TreeModel *treeModel() const override { return m_model; }

private:
    TreeModel *const m_model;
};

// ---------------------------------------------------------------------------------------------------------------------

template <typename ValueType>
class TreeModel::ValueNode : public Node
{
public:
    explicit ValueNode(const ValueType &value, const Node *parent)
        : Node{parent}
        , m_value{value}
    {}

    QVariant displayText() const override;
    QVariant value() const override { return QVariant::fromValue(m_value); }
    std::optional<DeviceDescription> device() const override { return Node::device(); }

    [[nodiscard]] operator const ValueType &() const { return m_value; }
    void update(const ValueType &value);

private:
    void updateChildren() {}

    ValueType m_value;
};

// ---------------------------------------------------------------------------------------------------------------------

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
    case Role::Device:
        return makeVariant(device());
    case Role::DeviceUrl:
        return deviceUrl();
    }

    return {};
}

std::optional<DeviceDescription> TreeModel::Node::device() const
{
    if (m_parent)
        return m_parent->device();

    return {};
}

QUrl TreeModel::Node::deviceUrl() const
{
    if (const auto optionalDevice = device())
        return optionalDevice->url;

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

template<class NodeType, typename ...Args>
NodeType *TreeModel::Node::addChild(Args &&...args)
{
    const auto model = treeModel();
    Q_ASSERT(model != nullptr);

    const auto newRow = static_cast<int>(m_children.size());
    model->beginInsertRows(model->indexForNode(this), newRow, newRow);

    qInfo() << "BEGIN INSERT ROWS"
            << QMetaType::fromType<decltype(this)>().name()
            << QMetaType::fromType<NodeType *>().name()
            << model->indexForNode(this).isValid()
            << model->indexForNode(this) << newRow;

    auto node = std::make_unique<NodeType>(std::forward<Args>(args)..., this);
    const auto nodePointer = node.get();
    m_children.emplace_back(std::move(node));

    model->endInsertRows();
    return nodePointer;
}

template<class NodeType, typename ...Args>
NodeType *TreeModel::Node::updateOrAddChild(Args &&...args, const Predicate &predicate)
{
    if (const auto child = dynamic_cast<NodeType *>(findChild(predicate))) {
        child->update(std::forward<Args>(args)...);
        return child;
    } else {
        return addChild<NodeType>(std::forward<Args>(args)...);
    }
}

template<class NodeType, auto IdField, typename ValueType>
NodeType *TreeModel::Node::updateOrAddChild(const ValueType &value)
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
void TreeModel::Node::updateOrAddChildren(const ContainerType &container)
{
    for (const auto &value : container)
        updateOrAddChild<NodeType, IdField>(value);
}

TreeModel::Node *TreeModel::Node::findChild(const std::function<bool(const Pointer &)> &predicate) const
{
    if (const auto it = std::find_if(m_children.cbegin(), m_children.cend(), predicate); it != m_children.cend())
        return it->get();

    return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------

template <>
QVariant TreeModel::ValueNode<TreeModel::DeviceType>::displayText() const
{
    return m_value.id;
}

// ---------------------------------------------------------------------------------------------------------------------

template <>
QVariant TreeModel::ValueNode<DeviceDescription>::displayText() const
{
    return m_value.uniqueDeviceName;
}

template <>
std::optional<DeviceDescription> TreeModel::ValueNode<DeviceDescription>::device() const
{
    return m_value;
}

template <>
void TreeModel::ValueNode<DeviceDescription>::updateChildren()
{
    updateOrAddChildren<ServiceNode, &ServiceDescription::id>(m_value.services);
}

// ---------------------------------------------------------------------------------------------------------------------

template <>
QVariant TreeModel::ValueNode<ServiceDescription>::displayText() const
{
    return m_value.type;
}

template <>
void TreeModel::ValueNode<ServiceDescription>::updateChildren()
{
    qInfo() << m_value.id << m_value.scpd.has_value();
    if (const auto &scpd = m_value.scpd) {
        qInfo() << m_value.id << scpd->actions.count() << scpd->stateVariables.count();
        updateOrAddChildren<ActionNode, &ActionDescription::name>(scpd->actions);
        updateOrAddChildren<VariableNode, &StateVariableDescription::name>(scpd->stateVariables);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

template <>
QVariant TreeModel::ValueNode<ActionDescription>::displayText() const
{
    return m_value.name;
}

// ---------------------------------------------------------------------------------------------------------------------

template <>
QVariant TreeModel::ValueNode<StateVariableDescription>::displayText() const
{
    return m_value.name;
}

// ---------------------------------------------------------------------------------------------------------------------

template <typename ValueType>
void TreeModel::ValueNode<ValueType>::update(const ValueType &value)
{
    const auto model = treeModel();
    Q_ASSERT(model != nullptr);

    m_value = value;

    const auto &modelIndex = model->indexForNode(this);
    emit model->dataChanged(modelIndex, modelIndex);

    updateChildren();
}

// ---------------------------------------------------------------------------------------------------------------------

TreeModel::TreeModel(QObject *parent)
    : QAbstractItemModel{parent}
    , m_root{new RootNode{this}}
    , m_resolver{new Resolver{this}}
{
    connect(m_resolver, &Resolver::deviceFound, this, [this](const DeviceDescription &device) {
        const auto deviceType = DeviceType{device.deviceType};
        const auto node = m_root->updateOrAddChild<DeviceTypeNode, &DeviceType::id>(deviceType);
        node->updateOrAddChild<DeviceNode, &DeviceDescription::uniqueDeviceName>(device);
    });

    connect(m_resolver, &Resolver::serviceLost, this, [](const auto &serviceName) {
        qCInfo(lcTreeModel) << "service lost:" << serviceName;
    });

    m_resolver->setBehavior(Resolver::Behavior::LoadServiceDescription);
}

void TreeModel::addQuery(const QString &serviceName)
{
    m_resolver->lookupService(serviceName);
}

void TreeModel::setNetworkAccessManager(QNetworkAccessManager *newManager)
{
    m_resolver->setNetworkAccessManager(newManager);
}

QNetworkAccessManager *TreeModel::networkAccessManager() const
{
    return m_resolver->networkAccessManager();
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

const TreeModel::Node *TreeModel::nodeForIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return m_root;
    else if (checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::DoNotUseParent))
        return static_cast<const Node *>(index.constInternalPointer());
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

} // namespace qnc::upnp
