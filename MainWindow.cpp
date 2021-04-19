#include "MainWindow.h"

#include <QDebug>
#include <QListWidgetItem>
#include <QStandardItemModel>
#include <QTimer>
#include <QtX11Extras/QX11Info>

#include "ui_MainWindow.h"

#include "Command.h"
#include "DiscoveryClient.h"
#include "DiscoveryService.h"
#include "TransparentMaximizedWindow.h"
#include "UvgRTP.h"
#include "X11Key.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , _streamer_socket(9000)
    , _streamer([this](uint32_t ip, const Command &cmd){
        _streamer_socket.SendCommand(ip, cmd);
    }, this)
    , _event_handler(this)
    , _frame_extractor([this](const Frame &frame, uint32_t ip){
        emit RenderImage(frame, ip);
    })
{
    ui->setupUi(this);
    setWindowTitle("Kingfisher Screen Controller");

    _node_model = new NodeModel(ui->listView);
    ui->listView->setModel(_node_model);
    ui->listView->show();
    qRegisterMetaType<uint32_t>("uint32_t");
    qRegisterMetaType<uint16_t>("uint16_t");

    connect(ui->listView, &QListView::doubleClicked,this,&MainWindow::NodeActivated);
    connect(ui->listView, &NodeListView::NodeActivated,this,&MainWindow::NodeActivated);
    connect(this, &MainWindow::DiscoveredNode, _node_model, &NodeModel::DiscoveredNode,
            Qt::ConnectionType::QueuedConnection);
    connect(this, &MainWindow::RenderImage, this, &MainWindow::OnRenderImage,
            Qt::ConnectionType::QueuedConnection);
    connect(&_node_name, &NodeNameDialog::NameChanged, [this](){ _node_name_changed = true; });
    connect(&_event_handler, &EventHandler::SetDecoderFrameWidthHeight, [this](uint32_t ip, uint32_t width, uint32_t height){
            _frame_extractor.SetDecoderFrameWidthHeight(ip, width, height);
    });
    connect(&_event_handler, &EventHandler::StartStreaming,
            &_streamer, &ScreenStreamer::StartStreaming,
            Qt::ConnectionType::QueuedConnection);
    connect(&_event_handler, &EventHandler::StopStreaming,
            this, &MainWindow::StopStreaming,
            Qt::ConnectionType::QueuedConnection);
    connect(&_event_handler, &EventHandler::StoppedStreaming,
            this, &MainWindow::DeleteTransparentWindowOverlay,
            Qt::ConnectionType::QueuedConnection);

    StartDiscoveryService();
    PrepareToReceiveStream();
}

MainWindow::~MainWindow()
{
    _stop = true;
    _discovery_thread.wait();
    for(auto &window : m_transparent_window)
    {
        delete window;
    }
    _node_model->deleteLater();
    delete ui;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
//    X11Key x11(this);
//    uint32_t key = 0;
//    uint32_t modifier = 0;
//    uint32_t type = 0;
//    for(int i = 0; i < 2; ++i)
//    {
//        x11.testKeyEvent(winId(), key, modifier, type);
//        qDebug() << x11.testKey('q', key) << x11.testAltModifier(modifier);
//    }
}

void MainWindow::StartDiscoveryService()
{
    _discovery_thread = std::async(std::launch::async, [this](){
        pthread_setname_np(pthread_self(), "discover");
        DiscoveryService service(_node_name.GetFileAbsolutePathName().toUtf8().data());
        DiscoveryClient client(_node_name.GetFileAbsolutePathName().toUtf8().data(),
                               [this](DiscoveryData *data, uint32_t ip, uint16_t port){
//            qDebug() << "Discovered node " << QString(data->m_name) << " at " << ip << SocketReader::IpToString(ip) << ":" << port;
            emit DiscoveredNode(QString(data->m_name), ip, port);
        });
        while(!_stop)
        {
            client.Discover();
            usleep(2 * 1000 * 1000);

            if(_node_name_changed)
            {
                _node_name_changed = false;
                service.UpdateDeviceId(_node_name.GetFileAbsolutePathName().toUtf8().data());
                client.UpdateDeviceId(_node_name.GetFileAbsolutePathName().toUtf8().data());
            }
        }
    });
}

void MainWindow::on_connectButtton_clicked()
{
    if(ui->listView->selectedIndexes().size() == 0)
    { return; }

    int row = ui->listView->selectedIndexes().first().row();
    if(row < 0) { return; }

    NodeActivated(_node_model->index(row, 0));
}

void MainWindow::SendStartStreamingCommand(uint32_t ip)
{
    _streamer_socket.SendCommand(ip, Command(Command::EventType::StartStreaming));
    _frame_extractor.ReadyToReceive(ip);
    if(m_transparent_window.find(ip) != m_transparent_window.end())
    {
        QImage screen_shot = _streamer.ScreenShot();
        m_transparent_window[ip]->Show(screen_shot.width(), screen_shot.height(), _streamer.ActiveScreen());
    }
}

void MainWindow::NodeActivated(QModelIndex index)
{
    uint32_t ip = _node_model->Ip(index);
    SendStartStreamingCommand(ip);
    if(_rtp) { _rtp.reset(); }
    _rtp = std::make_unique<UvgRTP>(ip, 8889, 8888, [this,ip](uint8_t* payload, size_t payload_len){
        _frame_extractor.ExtractFrame(payload, payload_len, ip);
    });
}

void MainWindow::StopStreaming(uint32_t ip)
{
    _streamer.StopStreaming(ip);
    _streamer_socket.SendCommand(ip, Command::EventType::StoppedStreaming);
}

void MainWindow::PrepareToReceiveStream()
{
    _streamer_socket.StartRecieveDataThread([this](const Command &cmd, uint32_t ip){
            _event_handler.HandleCommand(cmd, ip);
    });
}

void MainWindow::DeleteTransparentWindowOverlay(uint32_t ip)
{
    _frame_extractor.Stop(ip);
    if(m_transparent_window.find(ip) == m_transparent_window.end())
    {
        return;
    }
    TransparentMaximizedWindow *wnd = m_transparent_window[ip];
    wnd->close();
    wnd->deleteLater();
    m_transparent_window.remove(ip);
}

void MainWindow::OnRenderImage(const Frame &frame, uint32_t ip)
{
    if(m_transparent_window.find(ip) ==  m_transparent_window.end())
    {
        MakeNewTransparentWindowOverlay(ip);
    }

    if(!m_transparent_window[ip]->IsClosed())
    {
        m_transparent_window[ip]->SetImage(frame);
    }
}

void MainWindow::MakeNewTransparentWindowOverlay(uint32_t ip)
{
    if(_node_model->rowCount() == 0)
    {return;}

    m_transparent_window[ip] = new TransparentMaximizedWindow(
                _node_model->Name(ip), this);
    connect(m_transparent_window[ip], &TransparentMaximizedWindow::Close,
            [this, ip](){
        _streamer_socket.SendCommand(ip, Command::EventType::StopStreaming);
    });
    connect(m_transparent_window[ip], &TransparentMaximizedWindow::SendCommandToNode,
            [this, ip](const Command &pkt){
        if(m_transparent_window.find(ip) != m_transparent_window.end())
        {
            _streamer_socket.SendCommand(ip, pkt);
        }
    });
    QImage screen_shot = _streamer.ScreenShot();
    m_transparent_window[ip]->Show(screen_shot.width(), screen_shot.height(), _streamer.ActiveScreen());

}
