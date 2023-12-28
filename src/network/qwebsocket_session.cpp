#include <memory>
//
#include <QString>
#include <QThread>
//
#include <fmt/format.h>
#include <nlohmann/json.hpp>
//
#include "debug/print.hpp"
#include "network/qwebsocket_session.hpp"
#include "util/nljson.hpp"

// ----------------------------------------------------------------------------
using namespace grox::debug;
template <int Level>
static print_threshold<Level, 5> qsession_dbg("QSession");
// ----------------------------------------------------------------------------

namespace net::ws {

  // ----------------------------------------------------------------------------
  std::shared_ptr<qwebsocket_session> qwebsocket_session::create(
    const QString address, const QString subscription, const rx_msg_handler_type handler)
  {
    return std::make_shared<qwebsocket_session>(address, subscription, handler);
  }

  // ----------------------------------------------------------------------------
  std::shared_ptr<qwebsocket_session> qwebsocket_session::create(const std::string address,
    int port, const QString subscription, const rx_msg_handler_type handler)
  {
    QString addr = QStringLiteral("wss://") + QString::fromStdString(address) +
      QStringLiteral(":") + QString::number(port);
    return std::make_shared<qwebsocket_session>(addr, subscription, handler);
  }

  // ----------------------------------------------------------------------------
  std::shared_ptr<qwebsocket_session> qwebsocket_session::create(const std::string address,
    int port, util::string_t subscription, const rx_msg_handler_type handler)
  {
    QString addr = QStringLiteral("wss://") + QString::fromStdString(address) +
      QStringLiteral(":") + QString::number(port);
    QString req;
    util::from_json(subscription, req);
    return std::make_shared<qwebsocket_session>(addr, req, handler);
  }

  // ----------------------------------------------------------------------------
  qwebsocket_session::qwebsocket_session(
    const QString address, const QString subscription, const rx_msg_handler_type handler)
  {
    // create a client
    client_ = new qwebsocket_client(QUrl(address), subscription, handler);
    // create a thread and move client onto it
    thread_ = new QThread();
    client_->moveToThread(thread_);

    // when thread starts, call startconnection on the websocket client
    QObject::connect(
      thread_, &QThread::started, client_,
      [this]() {
        qsession_dbg<2>.debug(
          fmt::format("{:17s} Starting websocket connection", "QThread:started"));
        this->client_->startConnection();
      },
      Qt::DirectConnection);

    // when websocket client finishes, exit the thread
    QObject::connect(
      client_, &qwebsocket_client::finished, client_,
      [this]() {
        qsession_dbg<2>.debug(fmt::format("{:17s} invoking thread quit", "Qclient::finished"));
        QMetaObject::invokeMethod(this->thread_, "quit", Qt::DirectConnection);
      },
      Qt::DirectConnection);

    // print out when thread is destroyed
    QObject::connect(
      thread_, &QThread::destroyed, thread_,
      []() { qsession_dbg<2>.debug(fmt::format("{:17s}", "QThread::destroyed")); },
      Qt::DirectConnection);
    thread_->start();
  }

  // ----------------------------------------------------------------------------
  qwebsocket_session::~qwebsocket_session()
  {
    client_->stopConnection();
    thread_->wait();
    delete client_;
    delete thread_;
  }

}    // namespace net::ws
