#include "NodeListView.h"

NodeListView::NodeListView(QWidget *parent)
    : QListView(parent)
{

}

QModelIndexList NodeListView::selectedIndexes() const
{
    return QListView::selectedIndexes();
}
