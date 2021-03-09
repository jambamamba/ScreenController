#include "NodeModel.h"

#include <QString>

NodeModel::NodeModel(QObject *parent)
    : QStandardItemModel(parent)
{
}

QVariant NodeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() ||
            index.row() > m_nodes.size())
    {
        return QVariant();
    }

    switch(role)
    {
    case Qt::DisplayRole:
        return GetNodeAtPosition(index.row())->m_name;
    case Qt::DecorationRole:
        return QIcon(":/resources/laptop-icon-19517.png");
    case Qt::UserRole:
    {
        QVariant var;
        var.setValue(GetNodeAtPosition(index.row()));
        return var;
    }
    default:
       return QVariant();
    }
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
    if(m_nodes.find(ip) == m_nodes.end())
     {
         m_nodes.insert(ip, new Node(name, ip, port));
         emit dataChanged(index(0,0), index(m_nodes.size()-1,0), {Qt::DisplayRole, Qt::EditRole});
//         appendRow(new QStandardItem(QIcon(":/resources/laptop-icon-19517.png"), name));
     }
    else if(m_nodes[ip]->m_name != name)
    {
        m_nodes[ip]->m_name = name;
        emit dataChanged(index(0,0), index(m_nodes.size()-1,0), {Qt::DisplayRole, Qt::EditRole});
//        removeRows(0, rowCount());//todo get the correct item and update it only
//        appendRow(new QStandardItem(QIcon(":/resources/laptop-icon-19517.png"), name));
//               qDebug() << "update ist view with new name " << data->m_name;
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
            m_nodes.erase(it);
            delete (*it);
            emit dataChanged(index(0,0), index(m_nodes.size()-1,0), {Qt::DisplayRole, Qt::EditRole});
        }
        else
        {
            ++it;
            ++idx;
        }
    }
}
