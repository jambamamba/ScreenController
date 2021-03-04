#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "EventHandler.h"
#include "SocketReader.h"
#include "ScreenStreamer.h"
#include "NodeNameDialog.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#include <QModelIndex>

struct Node
{
    const std::string m_name;
    uint32_t m_ip;
    uint16_t m_port;
    Node(const std::string &name,
         uint32_t ip,
         uint16_t port)
        : m_name(name)
        , m_ip(ip)
        , m_port(port)
    {}
};

struct Command;
struct ImageConverterInterface;
class QStandardItemModel;
class NodeListModel;
class TransparentMaximizedWindow;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
signals:
    void StartPlayback(const QImage&img, uint32_t from_ip);
    void StoppedStreaming(uint32_t ip);

protected:
    void StartDiscoveryService();
    void PrepareToReceiveStream();
    void HandleCommand(const Command &pkt, uint32_t from_ip);
    void DeleteTransparentWindowOverlay(uint32_t ip);
    void SendStartStreamingCommand(uint32_t ip);

private slots:
    void ShowTransparentWindowOverlay(const QImage&img, uint32_t from_ip);
    void NodeDoubleClicked(QModelIndex);

    void on_connectButtton_clicked();

private:
    Ui::MainWindow *ui;
    NodeNameDialog m_node_name;
    QMap<uint32_t /*ip*/, TransparentMaximizedWindow*> m_transparent_window;
    QRect m_region;
    SocketReader m_streamer_socket;
    ScreenStreamer m_streamer;
    std::future<void> m_discovery_thread;
    QStandardItemModel *m_node_model;
    QMap<uint32_t /*ip*/, Node*> m_nodes;
    EventHandler m_event_handler;
    bool m_stop = false;
};
#endif // MAINWINDOW_H
