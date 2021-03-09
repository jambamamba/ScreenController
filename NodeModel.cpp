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
        return GetNode(index.row())->m_name;
    case Qt::DecorationRole:
        return QIcon(":/resources/laptop-icon-19517.png");
    case Qt::UserRole:
    {
        QVariant var;
        var.setValue(GetNode(index.row()));
        return var;
    }
    default:
       return QVariant();
    }
}

const NodeModel::Node *NodeModel::GetNode(size_t idx) const
{
    size_t pos = 0;
    for(const auto &node : m_nodes)
    {
        if(pos == idx)
        {
            return node;
        }
    }
    return nullptr;
}

uint32_t NodeModel::Ip(const QModelIndex &index) const
{
    return (index.row() < m_nodes.size()) ?
                GetNode(index.row())->m_ip : 0;
}

QString NodeModel::Name(uint32_t ip) const
{
    size_t idx = 0;
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

}
