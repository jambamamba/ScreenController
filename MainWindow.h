#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "SocketReader.h"
#include "ScreenStreamer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#include <QModelIndex>

struct Node
{
    const std::string m_name;
    const std::string m_ip;
    uint16_t m_port;
    Node(    const std::string &name,
    const std::string &ip,
    uint16_t port)
        : m_name(name)
        , m_ip(ip)
        , m_port(port)
    {}
};

class QStandardItemModel;
class NodeListModel;
class TransparentMaximizedWindow;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void captureFullScreen();
    void scrubScreenCaptureModeOperation();
    void StartDiscoveryService();
    void PrepareToReceiveStream();

private slots:
    void showTransparentWindowOverlay();
    void NodeDoubleClicked(QModelIndex);
    void on_testButton_clicked();

private:
    Ui::MainWindow *ui;
    TransparentMaximizedWindow *m_transparent_window;
    QRect m_region;
    SocketReader m_streamer_socket;
    ScreenStreamer m_streamer;
    std::future<void> m_discovery_thread;
    QStandardItemModel *m_node_model;
    QMap<QString, Node*> m_nodes;
    bool m_stop = false;
};
#endif // MAINWINDOW_H
