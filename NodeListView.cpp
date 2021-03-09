#include "NodeListView.h"

#include <QDebug>
#include <QKeyEvent>

NodeListView::NodeListView(QWidget *parent)
    : QListView(parent)
{

}

QModelIndexList NodeListView::selectedIndexes() const
{
    return QListView::selectedIndexes();
}

void NodeListView::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Return)
    {
        emit NodeActivated(selectedIndexes().first());
    }
}
