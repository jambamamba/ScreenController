#include "MainWindow.h"

#include <QDebug>
#include <QListWidgetItem>
#include <QStandardItemModel>

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
    , m_streamer(m_streamer_socket, this)
    , m_event_handler(this)
{
    ui->setupUi(this);
    setWindowTitle("Kingfisher Screen Controller");

    m_node_model = new QStandardItemModel(ui->listView);
    connect(ui->listView, &QListView::doubleClicked,this,&MainWindow::NodeDoubleClicked);

    connect(this, &MainWindow::StartPlayback,
            this, &MainWindow::ShowTransparentWindowOverlay,
            Qt::ConnectionType::QueuedConnection);

    connect(&m_event_handler, &EventHandler::StartStreaming,
            &m_streamer, &ScreenStreamer::StartStreaming,
            Qt::ConnectionType::QueuedConnection);
    connect(&m_event_handler, &EventHandler::StopStreaming,
            &m_streamer, &ScreenStreamer::StopStreaming,
            Qt::ConnectionType::QueuedConnection);
    connect(&m_event_handler, &EventHandler::StoppedStreaming,
            this, &MainWindow::DeleteTransparentWindowOverlay,
            Qt::ConnectionType::QueuedConnection);

    ui->listView->setModel(m_node_model);
    ui->listView->show();

    StartDiscoveryService();
    PrepareToReceiveStream();

    grabKeyboard();
//    static auto foo = std::async([this](){
//        X11Key x11(this);
//        uint32_t key = 0;
//        uint32_t modifier = 0;
//        int type = 0;
//        x11.testKeyEvent(winId(), key, modifier, type);
//        qDebug() << "x11 key" << key << modifier << type;
//    });
}

MainWindow::~MainWindow()
{
    m_stop = true;
    m_discovery_thread.wait();
    delete ui;
}

void MainWindow::StartDiscoveryService()
{
    m_discovery_thread = std::async(std::launch::async, [this](){
        pthread_setname_np(pthread_self(), "discover");
        DiscoveryService discovery(m_node_name.GetFileAbsolutePathName().toUtf8().data());
        DiscoveryClient client(m_node_name.GetFileAbsolutePathName().toUtf8().data(),
                               [this](DiscoveryData *data, std::string ip, uint16_t port){
//            qDebug() << "Discovered node " << QString(data->m_name) << " at " << QString(ip.c_str()) << ":" << port;
            uint32_t ip_ = SocketReader::IpFromString(ip.c_str());
           if(m_nodes.find(ip_) == m_nodes.end())
            {
                m_nodes.insert(ip_, new Node(data->m_name, ip_, port));
                m_node_model->appendRow(new QStandardItem(QIcon(":/resources/laptop-icon-19517.png"), data->m_name));
            }
        });
        while(!m_stop)
        {
            client.Discover();
            QApplication::processEvents();
            usleep(2 * 1000 * 1000);
        }
    });

}


void MainWindow::on_connectButtton_clicked()
{
    if(ui->listView->selectedIndexes().size() == 0)
    { return; }

    int idx = ui->listView->selectedIndexes().first().row();
    if(idx < 0)
    { return; }

    int i = 0;
    for(const auto &node : m_nodes)
    {
        if(i == idx)
        {
            SendStartStreamingCommand(node->m_ip);
        }
        ++i;
    }
}

void MainWindow::SendStartStreamingCommand(uint32_t ip)
{
    m_streamer.SendCommand(ip, Command::EventType::StartStreaming);
    m_streamer_socket.Start(ip);
    if(m_transparent_window.find(ip) != m_transparent_window.end())
    {
        m_transparent_window[ip]->ReOpen();
    }
}

void MainWindow::NodeDoubleClicked(QModelIndex index)
{
    int idx = 0;
    for(const auto &node     : m_nodes)
    {
        if(idx == index.row())
        {
            SendStartStreamingCommand(node->m_ip);
            break;
        }
        idx ++;
    }
}

void MainWindow::PrepareToReceiveStream()
{
    m_streamer_socket.StartRecieveDataThread([this](const Command &pkt, uint32_t ip){
        m_event_handler.HandleCommand(pkt, ip);
    });
    m_streamer_socket.PlaybackImages([this](const QImage&img, uint32_t ip) {
        emit StartPlayback(img, ip);
    });
}

void MainWindow::DeleteTransparentWindowOverlay(uint32_t ip)
{
    m_streamer_socket.Stop(ip);
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

void MainWindow::ShowTransparentWindowOverlay(const QImage &img, uint32_t ip)
{
    if(m_transparent_window.find(ip) !=  m_transparent_window.end())
    {
        if(!m_transparent_window[ip]->IsClosed())
        {
            m_transparent_window[ip]->SetImage(img);
        }
        return;
    }

    if(m_nodes.find(ip) == m_nodes.end())
    {return;}

    m_transparent_window[ip] = new TransparentMaximizedWindow(
                m_nodes[ip]->m_name.c_str(), this);
    connect(m_transparent_window[ip], &TransparentMaximizedWindow::Close,
            [this, ip](){
        m_streamer.SendCommand(ip, Command::EventType::StopStreaming);
        if(m_transparent_window.find(ip) != m_transparent_window.end())
        {
            m_transparent_window[ip]->hide();
        }
    });
    connect(m_transparent_window[ip], &TransparentMaximizedWindow::SendCommandToNode,
            [this, ip](const Command &pkt){
        if(m_transparent_window.find(ip) != m_transparent_window.end())
        {
            m_streamer.SendCommand(ip, pkt);
        }
    });
    QImage screen_shot = m_streamer.ScreenShot();
    m_transparent_window[ip]->Show(screen_shot.width(), screen_shot.height(), m_streamer.ActiveScreen());
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    //todo : do this in transparent window in loop and use select on keyboard device id to wait to read
    X11Key x11(this);
    uint32_t key = 0;
    uint32_t modifier = 0;
    int type = 0;
    x11.testKeyEvent(winId(), key, modifier, type);
    qDebug() << "x11 key" << key << modifier << type;

}
