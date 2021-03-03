#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "SocketReader.h"
#include "ScreenStreamer.h"
#include "NodeNameDialog.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#include <QModelIndex>
#include "CommandMessage.h"

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
    void StartStreaming(uint32_t ip, int decoder_type);

protected:
    void StartDiscoveryService();
    void PrepareToReceiveStream();
    void HandleCommand(const CommandMessage::Packet &pkt, uint32_t from_ip);

private slots:
    void ShowTransparentWindowOverlay(const QImage&img, uint32_t from_ip);
    void NodeDoubleClicked(QModelIndex);

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
    bool m_stop = false;
};
#endif // MAINWINDOW_H
