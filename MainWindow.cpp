#include "MainWindow.h"

#include <QDebug>
#include <QListWidgetItem>
#include <QStandardItemModel>

#include "ui_MainWindow.h"

#include "DiscoveryService.h"
#include "DiscoveryClient.h"
#include "TransparentMaximizedWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_streamer_socket(9000)
    , m_streamer(m_streamer_socket)
{
    ui->setupUi(this);
    setWindowTitle("Kingfisher Screen Controller");

    m_node_model = new QStandardItemModel(ui->listView);
    connect(ui->listView, &QListView::doubleClicked,this,&MainWindow::NodeDoubleClicked);

    connect(this, &MainWindow::StartPlayback,
            this, &MainWindow::ShowTransparentWindowOverlay,
            Qt::ConnectionType::QueuedConnection);

    ui->listView->setModel(m_node_model);
    ui->listView->show();

    StartDiscoveryService();
    PrepareToReceiveStream();
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
            usleep(3 * 1000 * 1000);
        }
    });

}

void MainWindow::NodeDoubleClicked(QModelIndex index)
{
    int idx = 0;
    for(const auto &node : m_nodes)
    {
        if(idx == index.row())
        {
            m_streamer.StartStreaming(
                        SocketReader::IpToString(node->m_ip),
                        node->m_port);
            break;
        }
        idx ++;
    }
}

void MainWindow::PrepareToReceiveStream()
{
    m_streamer_socket.StartRecieveDataThread();
    m_streamer_socket.PlaybackImages([this](const QImage&img, uint32_t ip) {
        if(m_transparent_window.find(ip) ==  m_transparent_window.end())
        {
            emit StartPlayback(ip);
        }
        else
        {
            m_transparent_window[ip]->SetImage(img);
        }
    });
}

void MainWindow::ShowTransparentWindowOverlay(uint32_t ip)
{
    if(m_transparent_window.find(ip) !=  m_transparent_window.end())
    {return;}

    if(m_nodes.find(ip) == m_nodes.end())
    {return;}

    m_transparent_window[ip] = new TransparentMaximizedWindow(
                m_nodes[ip]->m_name.c_str(), this);
    connect(m_transparent_window[ip], &TransparentMaximizedWindow::Close,
            [this, ip](){
        if(m_transparent_window.find(ip) != m_transparent_window.end())
        {
            TransparentMaximizedWindow *wnd = m_transparent_window[ip];
            wnd->hide();
            wnd->deleteLater();
            m_transparent_window.remove(ip);
        }
    });
    QImage screen_shot = m_streamer.ScreenShot();
    m_transparent_window[ip]->Show(screen_shot.width(), screen_shot.height(), m_streamer.ActiveScreen());
}
