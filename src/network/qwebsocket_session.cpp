#include <memory>
#include <string>
//
#include <fmt/format.h>
#include <nlohmann/json.hpp>
//
#include <QString>
#include <QThread>
//
#include "debug/print.hpp"
#include "network/qwebsocket_session.hpp"
#include "util/json_qstring.hpp"

// ----------------------------------------------------------------------------
using namespace grox::debug::detail;
template <int Level>
inline constexpr print_threshold<Level, 5> qsession_dbg("QSession");
// ----------------------------------------------------------------------------

namespace net::ws {

  // ----------------------------------------------------------------------------
  std::shared_ptr<qwebsocket_session> qwebsocket_session::create(std::string const& id,
      QString const address, QString const subscription, rx_msg_handler_type const handler)
  {
    return std::make_shared<qwebsocket_session>(id, address, subscription, handler);
  }

  // ----------------------------------------------------------------------------
  std::shared_ptr<qwebsocket_session> qwebsocket_session::create(std::string const& id,
      std::string const& address, int port, QString const subscription,
      rx_msg_handler_type const handler)
  {
    QString addr = QStringLiteral("wss://") + QString::fromStdString(address) +
        QStringLiteral(":") + QString::number(port);
    return std::make_shared<qwebsocket_session>(id, addr, subscription, handler);
  }

  // ----------------------------------------------------------------------------
  std::shared_ptr<qwebsocket_session> qwebsocket_session::create(std::string const& id,
      std::string const& address, int port, util::string_t subscription,
      rx_msg_handler_type const handler)
  {
    QString addr = QStringLiteral("wss://") + QString::fromStdString(address) +
        QStringLiteral(":") + QString::number(port);
    QString req;
    util::from_json(subscription, req);
    return std::make_shared<qwebsocket_session>(id, addr, req, handler);
  }

  // ----------------------------------------------------------------------------
  qwebsocket_session::qwebsocket_session(std::string const& id, QString const address,
      QString const subscription, rx_msg_handler_type const handler)
  {
    // create a client
    client_ = new qwebsocket_client(id, QUrl(address), subscription, handler);
    // create a thread and move client onto it
    thread_ = new QThread();
    client_->moveToThread(thread_);

    // when thread starts, call startconnection on the websocket client
    QObject::connect(
        thread_, &QThread::started, client_,
        [this]() {
          qsession_dbg<2>.debug(fmt::format("{:20s} {} {} Starting websocket connection",
              client_->id(), fmt::ptr(this), "QThread:started"));
          this->client_->startConnection();
        },
        Qt::DirectConnection);

    // when websocket client finishes, exit the thread
    QObject::connect(
        client_, &qwebsocket_client::finished, client_,
        [this]() {
          qsession_dbg<2>.debug(
              fmt::format("{:20s} {} invoking thread quit", client_->id(), "Qclient::finished"));
          QMetaObject::invokeMethod(this->thread_, "quit", Qt::DirectConnection);
        },
        Qt::DirectConnection);

    // print out when thread is destroyed
    QObject::connect(
        thread_, &QThread::destroyed, thread_,
        [id = client_->id()]() {
          qsession_dbg<2>.debug(fmt::format("{:20s} {}", id, "QThread::destroyed"));
        },
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
