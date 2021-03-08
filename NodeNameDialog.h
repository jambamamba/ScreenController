#pragma once

#include <QDialog>

namespace Ui {
class NodeNameDialog;
}

class NodeNameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NodeNameDialog(QWidget *parent = nullptr);
    ~NodeNameDialog();
    QString GetFileAbsolutePathName() const;
    QString GetName() const;
    void SetName(const QString& name = "");
    virtual void accept() override;
    virtual void reject() override;

signals:
    void NameChanged();

private:
    Ui::NodeNameDialog *ui;
};
