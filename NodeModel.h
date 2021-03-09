#pragma once

#include <QMap>
#include <QAbstractItemModel>
#include <QVariant>

class NodeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    struct Node
    {
        QString m_name;
        uint32_t m_ip;
        uint16_t m_port;
        std::chrono::steady_clock::time_point m_updated_at = std::chrono::steady_clock::now();
        Node(const QString &name,
             uint32_t ip,
             uint16_t port)
            : m_name(name)
            , m_ip(ip)
            , m_port(port)
        {}
        //required for Q_DECLARE_METATYPE below
        Node() = default;
        ~Node() = default;
        Node(const Node &) = default;
        Node &operator=(const Node &) = default;
    };

    NodeModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &index) const override;
    uint32_t Ip(const QModelIndex &index) const;
    QString Name(uint32_t ip) const;

public slots:
    void DiscoveredNode(const QString &name, uint32_t ip, uint16_t port);

protected:
    const NodeModel::Node *GetNodeAtPosition(size_t pos) const;
    void RemoveStaleNodes();

    QMap<uint32_t /*ip*/, Node*> m_nodes;
};
Q_DECLARE_METATYPE(const NodeModel::Node*);
Q_DECLARE_METATYPE(NodeModel::Node*);
