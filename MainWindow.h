#pragma once

#include <QMainWindow>
#include <QModelIndex>
#include <QTimer>

#include "EventHandler.h"
#include "FrameExtractor.h"
#include "SocketTrasceiver.h"
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
class UvgRTP;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    virtual void keyPressEvent(QKeyEvent *event) override;
signals:
    void StoppedStreaming(uint32_t ip);
    void DiscoveredNode(const QString &name, uint32_t ip, uint16_t port);
    void RenderImage(const Frame &frame, uint32_t ip);

protected:
    void StartDiscoveryService();
    void PrepareToReceiveStream();
    void HandleCommand(const Command &pkt, uint32_t from_ip);
    void DeleteTransparentWindowOverlay(uint32_t ip);
    void SendStartStreamingCommand(uint32_t ip);
    void MakeNewTransparentWindowOverlay(uint32_t ip);

private slots:
    void OnRenderImage(const Frame &frame, uint32_t ip);
    void NodeActivated(QModelIndex);
    void StopStreaming(uint32_t ip);
    void on_connectButtton_clicked();

private:
    Ui::MainWindow *ui;
    NodeNameDialog _node_name;
    QMap<uint32_t /*ip*/, TransparentMaximizedWindow*> m_transparent_window;
    QRect _region;
    SocketTrasceiver _streamer_socket;
    std::unique_ptr<UvgRTP> _rtp;
    ScreenStreamer _streamer;
    std::future<void> _discovery_thread;
    NodeModel *_node_model;
    EventHandler _event_handler;
    FrameExtractor _frame_extractor;
    std::atomic<bool>_node_name_changed = false;
    bool _stop = false;
};
