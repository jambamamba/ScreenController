#include "NodeModel.h"

#include <QDebug>
#include <QIcon>
#include <QString>

NodeModel::NodeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

int NodeModel::rowCount(const QModelIndex &parent) const
{
    qDebug() << "rowCount " << m_nodes.size();
    return m_nodes.size() > 0 ? m_nodes.size() : 1;//without initial 1, it never draws anything!
}

int NodeModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant NodeModel::data(const QModelIndex &index, int role) const
{
    qDebug() << "data " << index.row() << m_nodes.size() ;
    if (!index.isValid() ||
            index.row() >= m_nodes.size())
    {
        return QVariant();
    }

    const Node *node = GetNodeAtPosition(index.row());
    switch(role)
    {
    case Qt::DisplayRole:
        return node ? node->m_name : "";
    case Qt::DecorationRole:
        return node ? QIcon(":/resources/laptop-icon-19517.png") : QIcon();
    case Qt::UserRole:
    {
        QVariant var;
        var.setValue(node);
        return var;
    }
    default:
       return QVariant();
    }
}

bool NodeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    qDebug() << "setData";
    Node *node = qvariant_cast<Node *>(value);
    m_nodes.insert(node->m_ip, node);
    emit dataChanged(index, index,
                QVector<int>() << Qt::DisplayRole << Qt::EditRole);
    return true;
}

QModelIndex NodeModel::index(int row, int column, const QModelIndex &parent) const
{
    return QAbstractItemModel::createIndex(row, column, (void*)GetNodeAtPosition(row));
}

QModelIndex NodeModel::parent(const QModelIndex &index) const
{
//    return QAbstractItemModel::createIndex(0, 0, nullptr);
    return QModelIndex();
}

const NodeModel::Node *NodeModel::GetNodeAtPosition(size_t pos) const
{
    size_t idx = 0;
    for(const auto &node : m_nodes)
    {
        if(idx == pos)
        {
            return node;
        }
    }
    return nullptr;
}


uint32_t NodeModel::Ip(const QModelIndex &index) const
{
    return (index.row() < m_nodes.size()) ?
                GetNodeAtPosition(index.row())->m_ip : 0;
}

QString NodeModel::Name(uint32_t ip) const
{
    for(const auto &node : m_nodes)
    {
        if(node->m_ip == ip)
        {
            return node->m_name;
        }
    }
    return "";
}

void NodeModel::DiscoveredNode(const QString &name, uint32_t ip, uint16_t port)
{
    qDebug() << "discovered node " << name;
    if(m_nodes.find(ip) == m_nodes.end())
    {
//        QVariant var;
//        var.setValue(new Node(name, ip, port));
//        setData(index(m_nodes.size()-1, 0), var);

        Node *node = new Node(name, ip, port);
        m_nodes.insert(node->m_ip, node);

        insertRow(m_nodes.size());
        int f = rowCount();
        emit dataChanged(index(m_nodes.size()-1, 0),
                         index(m_nodes.size()-1, 0),
                    QVector<int>() << Qt::DisplayRole << Qt::EditRole);
    }
    else if(m_nodes[ip]->m_name != name)
    {
        m_nodes[ip]->m_name = name;
        emit dataChanged(index(m_nodes.size()-1,0),
                         index(m_nodes.size()-1,0),
        {Qt::DisplayRole, Qt::EditRole});
    }
    else
    {
        m_nodes[ip]->m_updated_at = std::chrono::steady_clock::now();
    }

    RemoveStaleNodes();
}

void NodeModel::RemoveStaleNodes()
{
    size_t idx = 0;
    for(auto it = m_nodes.begin(); it != m_nodes.end();)
    {
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(end - (*it)->m_updated_at).count();
        if(elapsed > 10)
        {
            Node *node = *it;
            it = m_nodes.erase(it);
            delete node;
            emit dataChanged(index(0,0), index(m_nodes.size()-1,0), {Qt::DisplayRole, Qt::EditRole});
        }
        else
        {
            ++it;
            ++idx;
        }
    }
}
