#pragma once

#include <QListView>

class NodeListView : public QListView
{
    Q_OBJECT

public:
    NodeListView(QWidget *parent);
    QModelIndexList selectedIndexes() const Q_DECL_OVERRIDE;
    virtual void keyPressEvent(QKeyEvent *event) override;
signals:
    void NodeActivated(QModelIndex index);
};
