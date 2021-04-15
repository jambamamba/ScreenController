#include "MainWindow.h"

#include <QDebug>
#include <QListWidgetItem>
#include <QStandardItemModel>
#include <QTimer>

#include "ui_MainWindow.h"

#include "DiscoveryService.h"
#include "DiscoveryClient.h"
#include "TransparentMaximizedWindow.h"

#include "Command.h"
#include "JpegConverter.h"
#include "WebPConverter.h"

#include "X11Key.h"
#include <QtX11Extras/QX11Info>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_streamer_socket(9000)
    , m_streamer([this](uint32_t ip, const Command &cmd){
        m_streamer_socket.SendCommand(ip, cmd);
    }, this)
    , m_event_handler(this)
    , m_frame_request_timer(new QTimer(this))
{
    ui->setupUi(this);
    setWindowTitle("Kingfisher Screen Controller");

    m_node_model = new NodeModel(ui->listView);
    ui->listView->setModel(m_node_model);
    ui->listView->show();
    qRegisterMetaType<uint32_t>("uint32_t");
    qRegisterMetaType<uint16_t>("uint16_t");

    connect(ui->listView, &QListView::doubleClicked,this,&MainWindow::NodeActivated);
    connect(ui->listView, &NodeListView::NodeActivated,this,&MainWindow::NodeActivated);
    connect(this, &MainWindow::DiscoveredNode, m_node_model, &NodeModel::DiscoveredNode,
            Qt::ConnectionType::QueuedConnection);
    connect(&m_node_name, &NodeNameDialog::NameChanged, [this](){ m_node_name_changed = true; });
    connect(this, &MainWindow::StartPlayback,
            this, &MainWindow::ShowTransparentWindowOverlay,
            Qt::ConnectionType::QueuedConnection);
    connect(&m_event_handler, &EventHandler::StartStreaming,
            &m_streamer, &ScreenStreamer::StartStreaming,
            Qt::ConnectionType::QueuedConnection);
    connect(&m_event_handler, &EventHandler::StopStreaming,
            this, &MainWindow::StopStreaming,
            Qt::ConnectionType::QueuedConnection);
    connect(&m_event_handler, &EventHandler::StoppedStreaming,
            this, &MainWindow::DeleteTransparentWindowOverlay,
            Qt::ConnectionType::QueuedConnection);
    connect(this, &MainWindow::RestartRequestNextFrameTimer,
            this, &MainWindow::OnRestartRequestNextFrameTimer,
            Qt::ConnectionType::QueuedConnection);
    connect(m_frame_request_timer, &QTimer::timeout,
            this, &MainWindow::RequestNextFrameTimerEvent);
    m_frame_request_timer->setSingleShot(true);

    StartDiscoveryService();
    PrepareToReceiveStream();
}

MainWindow::~MainWindow()
{
    m_stop = true;
    m_discovery_thread.wait();
    for(auto &window : m_transparent_window)
    {
        delete window;
    }
    m_node_model->deleteLater();
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
    m_discovery_thread = std::async(std::launch::async, [this](){
        pthread_setname_np(pthread_self(), "discover");
        DiscoveryService service(m_node_name.GetFileAbsolutePathName().toUtf8().data());
        DiscoveryClient client(m_node_name.GetFileAbsolutePathName().toUtf8().data(),
                               [this](DiscoveryData *data, uint32_t ip, uint16_t port){
//            qDebug() << "Discovered node " << QString(data->m_name) << " at " << ip << SocketReader::IpToString(ip) << ":" << port;
            emit DiscoveredNode(QString(data->m_name), ip, port);
        });
        while(!m_stop)
        {
            client.Discover();
            usleep(2 * 1000 * 1000);

            if(m_node_name_changed)
            {
                m_node_name_changed = false;
                service.UpdateDeviceId(m_node_name.GetFileAbsolutePathName().toUtf8().data());
                client.UpdateDeviceId(m_node_name.GetFileAbsolutePathName().toUtf8().data());
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

    NodeActivated(m_node_model->index(row, 0));
}

void MainWindow::SendStartStreamingCommand(uint32_t ip)
{
    m_streamer_socket.SendCommand(ip,
                                  Command(Command::EventType::StartStreaming,
                                          0,
                                          (int)m_streamer._default_decoder));
    m_frame_extractor.ReadyToReceive(ip);
    if(m_transparent_window.find(ip) != m_transparent_window.end())
    {
        QImage screen_shot = m_streamer.ScreenShot();
        m_transparent_window[ip]->Show(screen_shot.width(), screen_shot.height(), m_streamer.ActiveScreen());
    }
}

void MainWindow::NodeActivated(QModelIndex index)
{
    SendStartStreamingCommand(m_node_model->Ip(index));
}

void MainWindow::StopStreaming(uint32_t ip)
{
    m_streamer.StopStreaming(ip);
    m_streamer_socket.SendCommand(ip, Command::EventType::StoppedStreaming);
}

void MainWindow::PrepareToReceiveStream()
{
    m_streamer_socket.StartRecieveDataThread([this](const Command &cmd, uint32_t ip){
        switch(cmd.m_event)
        {
        case Command::EventType::FrameInfo:
        {
            uint32_t next_frame_num = m_frame_extractor.ExtractFrame(
                        (uint8_t*)(cmd.m_tail_bytes),
                        cmd.u.m_frame,
                        ip);

            m_streamer_socket.SendCommand(ip,
                                          Command(Command::EventType::StartStreaming,
                                                  next_frame_num,
                                                  (int)ImageConverterInterface::Types::X265));
            emit RestartRequestNextFrameTimer(next_frame_num, ip);
            break;
        }
        default:
            m_event_handler.HandleCommand(cmd, ip);
            break;
        }
    });
    m_frame_extractor.PlaybackImages([this](const Frame &frame, uint32_t ip) {
        emit StartPlayback(frame, ip);
    });
}

void MainWindow::OnRestartRequestNextFrameTimer(uint32_t next_frame_num, uint32_t ip)
{
    m_frame_request_timer->stop();
    if(_next_frame_request_data._next_frame_num != next_frame_num)
    {
        _next_frame_request_data.Set(next_frame_num, ip);
        m_frame_request_timer->start(2000);
    }
}

void MainWindow::RequestNextFrameTimerEvent()
{
    m_streamer_socket.SendCommand(_next_frame_request_data._ip,
                                  Command(Command::EventType::StartStreaming,
                                          _next_frame_request_data._next_frame_num,
                                          (int)ImageConverterInterface::Types::X265));
}

void MainWindow::DeleteTransparentWindowOverlay(uint32_t ip)
{
    m_frame_extractor.Stop(ip);
    return;//todo
    if(m_transparent_window.find(ip) == m_transparent_window.end())
    {
        return;
    }
    TransparentMaximizedWindow *wnd = m_transparent_window[ip];
    wnd->close();
    wnd->deleteLater();
    m_transparent_window.remove(ip);
}

void MainWindow::ShowTransparentWindowOverlay(const Frame &frame, uint32_t ip)
{
    if(m_transparent_window.find(ip) !=  m_transparent_window.end())
    {
        if(!m_transparent_window[ip]->IsClosed())
        {
            m_transparent_window[ip]->SetImage(frame);
        }
        return;
    }
    MakeNewTransparentWindowOverlay(ip);
}

void MainWindow::MakeNewTransparentWindowOverlay(uint32_t ip)
{
    if(m_node_model->rowCount() == 0)
    {return;}

    m_transparent_window[ip] = new TransparentMaximizedWindow(
                m_node_model->Name(ip), this);
    connect(m_transparent_window[ip], &TransparentMaximizedWindow::Close,
            [this, ip](){
        m_streamer_socket.SendCommand(ip, Command::EventType::StopStreaming);
    });
    connect(m_transparent_window[ip], &TransparentMaximizedWindow::SendCommandToNode,
            [this, ip](const Command &pkt){
        if(m_transparent_window.find(ip) != m_transparent_window.end())
        {
            m_streamer_socket.SendCommand(ip, pkt);
        }
    });
    QImage screen_shot = m_streamer.ScreenShot();
    m_transparent_window[ip]->Show(screen_shot.width(), screen_shot.height(), m_streamer.ActiveScreen());

}
