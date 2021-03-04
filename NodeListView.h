#pragma once

#include <QListView>

class NodeListView : public QListView
{
public:
    NodeListView(QWidget *parent);
    QModelIndexList selectedIndexes() const Q_DECL_OVERRIDE;

};
