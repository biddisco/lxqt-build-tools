#include <iostream>
#include <memory>
#include <thread>
//
#include <QStringLiteral>
#include <QThread>
#include <QtCore/QCoreApplication>
//
#include "network/qwebsocket_client.hpp"
#include "network/qwebsocket_session.hpp"

std::atomic<int> counter{0};
const int test_seconds = 5;

// ------------------------------------------------------------------
void onTextMessageReceived(QString message)
{
  qDebug() << "TextMessage received:" << message;
  counter++;
}

// ------------------------------------------------------------------
int main(int argc, char* argv[])
{
  QCoreApplication a(argc, argv);

  QString address = QStringLiteral("wss://ws.bitstamp.net:443");
  QString subscribe =
      "{\"event\": \"bts:subscribe\",\"data\": {\"channel\": \"order_book_btcusd\"}}";

  std::shared_ptr<net::ws::qwebsocket_session> websocket = net::ws::qwebsocket_session::create(
      "Bitstamp Trades", address, subscribe, net::ws::rx_msg_handler_type(onTextMessageReceived));

  int completed = 0;
  // wait N seconds and collect some data
  for (int i = 0; i < test_seconds && (counter.load() < 2); i++)
  {
    std::cout << "Closing in " << test_seconds - i << " seconds " << std::endl;
    std::chrono::seconds dura(1);
    std::this_thread::sleep_for(dura);
  }

  websocket.reset();
  return counter.load() > 1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
