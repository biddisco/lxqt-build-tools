#include <iostream>
#include <memory>
#include <thread>
//
#include <QStringLiteral>
#include <QThread>
#include <QtCore/QCoreApplication>
//
#include "network/qwebsocket_client.hpp"

std::atomic<int> counter{0};
const int test_seconds = 5;

void onTextMessageReceived(QString message)
{
  qDebug() << "TextMessage received:" << message;
}

struct websocket_wrapper
{
  qwebsocket_client* client_;
  QThread* thread_;

  websocket_wrapper(const QString address, const QString subscription)
  {
    // create a client
    client_ = new qwebsocket_client(
      QUrl(address), subscription, qwebsocket_client::msg_received_type(&onTextMessageReceived));
    // create a thread and move client onto it
    thread_ = new QThread();
    client_->moveToThread(thread_);

    // when thread starts, call startconnection on the websocket client
    QObject::connect(
      thread_, &QThread::started, client_,
      [this]() {
        qDebug() << "QThread:started : Starting websocket connection";
        this->client_->startConnection();
      },
      Qt::DirectConnection);

    // when websocket client finishes, exit the thread
    QObject::connect(
      client_, &qwebsocket_client::finished, client_,
      [this]() {
        qDebug() << "received client::finished : invoking thread quit";
        QMetaObject::invokeMethod(this->thread_, "quit", Qt::DirectConnection);
      },
      Qt::DirectConnection);

    // print out when thread is destroyed
    QObject::connect(
      thread_, &QThread::destroyed, thread_, []() { qDebug() << "QThread::destroyed"; },
      Qt::DirectConnection);
    thread_->start();
  }

  ~websocket_wrapper()
  {
    client_->stopConnection();
    thread_->wait();
    delete client_;
    delete thread_;
  }
};

int main(int argc, char* argv[])
{
  QCoreApplication a(argc, argv);

  QString address = QStringLiteral("wss://ws.bitstamp.net:443");
  QString subscribe =
    "{\"event\": \"bts:subscribe\",\"data\": {\"channel\": \"live_trades_btcusd\"}}";

  std::shared_ptr<websocket_wrapper> websocket =
    std::make_shared<websocket_wrapper>(address, subscribe);

  int completed = 0;
  // wait N seconds and collect some data
  for (int i = 0; i < test_seconds && (counter.load() == 0); i++)
  {
    std::cout << "Closing in " << test_seconds - i << " seconds " << std::endl;
    std::chrono::seconds dura(1);
    std::this_thread::sleep_for(dura);
  }

  websocket.reset();
}
