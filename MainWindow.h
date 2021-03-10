#pragma once

#include <QMainWindow>
#include <QModelIndex>

#include "EventHandler.h"
#include "SocketReader.h"
#include "ScreenStreamer.h"
#include "NodeNameDialog.h"
#include "NodeModel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

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
    virtual void keyPressEvent(QKeyEvent *event) override;
signals:
    void StartPlayback(const Frame &frame, uint32_t from_ip);
    void StoppedStreaming(uint32_t ip);
    void DiscoveredNode(const QString &name, uint32_t ip, uint16_t port);

protected:
    void StartDiscoveryService();
    void PrepareToReceiveStream();
    void HandleCommand(const Command &pkt, uint32_t from_ip);
    void DeleteTransparentWindowOverlay(uint32_t ip);
    void SendStartStreamingCommand(uint32_t ip);
    void MakeNewTransparentWindowOverlay(uint32_t ip);

private slots:
    void ShowTransparentWindowOverlay(const Frame &frame, uint32_t from_ip);
    void NodeActivated(QModelIndex);

    void on_connectButtton_clicked();

private:
    Ui::MainWindow *ui;
    NodeNameDialog m_node_name;
    QMap<uint32_t /*ip*/, TransparentMaximizedWindow*> m_transparent_window;
    QRect m_region;
    SocketReader m_streamer_socket;
    ScreenStreamer m_streamer;
    std::future<void> m_discovery_thread;
    NodeModel *m_node_model;
    EventHandler m_event_handler;
    std::atomic<bool>m_node_name_changed = false;
    bool m_stop = false;
};
