#include "NodeNameDialog.h"
#include "ui_NodeNameDialog.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLineEdit>
#include <QInputDialog>
#include <QStandardPaths>
#include <QString>
#include <QSysInfo>

namespace {
QString GetDirName()
{
    QString cache_dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return cache_dir + QDir::separator() + "kingfisher-screen-controller";
}
}//namespace


NodeNameDialog::NodeNameDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NodeNameDialog)
{
    ui->setupUi(this);
    setWindowTitle("Kingfisher Screen Controller");

    connect(this, &QDialog::finished, [this](int result){});

    qDebug() << "Looking for machine name in " << GetFileAbsolutePathName();
    QFile node_file(GetFileAbsolutePathName());
    if(node_file.exists() && GetName().size() > 0)
    { return; }

    SetName();
}

NodeNameDialog::~NodeNameDialog()
{
    delete ui;
}

QString NodeNameDialog::GetFileAbsolutePathName() const
{
    return GetDirName() + QDir::separator() + "nodename";
}

QString NodeNameDialog::GetName() const
{
    QFile node_file(GetFileAbsolutePathName());
    if(node_file.exists())
    { return QSysInfo::machineHostName(); }

    return node_file.readLine(128);
}

void NodeNameDialog::SetName(const QString &name)
{
    QDir dir;
    if(!dir.mkpath(GetDirName()))
    {
        qDebug() << "Failed to mkpath " << GetDirName();
        return;
    }

    if(name.size() > 0)
    {
        ui->lineEdit->setText(name);
        accept();
        emit NameChanged();
    }
    else
    {
        open();
    }
}

void NodeNameDialog::accept()
{
    QFile node_file(GetFileAbsolutePathName());
    if(!node_file.open(QIODevice::WriteOnly|QIODevice::Truncate))
    {
        qDebug() << "Failed to open file for writing " << GetFileAbsolutePathName();
        return;
    }
    node_file.write((QString("serialnumber:123")).toUtf8());
    node_file.write((QString("\n")).toUtf8());
    node_file.write((QString("name:") + ui->lineEdit->text()).toUtf8());
    QDialog::accept();
}

void NodeNameDialog::reject()
{
    ui->lineEdit->setText("KingFisher1");
    accept();
    emit NameChanged();
}
