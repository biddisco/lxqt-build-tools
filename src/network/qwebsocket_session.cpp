#include <memory>
#include <string>
//
#include <fmt/format.h>
#include <nlohmann/json.hpp>
//
#include <QString>
#include <QThread>
//
#include "debug/logging.hpp"
#include "network/qwebsocket_session.hpp"
#include "util/json_qstring.hpp"

// ----------------------------------------------------------------------------
static auto qsession_log = grox::log::create("QSession");
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
          GROX_LOG_DEBUG(qsession_log, "{:20s} {} {} Starting websocket connection", client_->id(),
              fmt::ptr(this), "QThread:started");
          this->client_->startConnection();
        },
        Qt::DirectConnection);

    QObject::connect(
        thread_, &QThread::finished, client_,
        [this, thread = thread_, client = client_]() {
          // note that "this" might already have destructed, it is not safe to call members
          // we copy the thread and client pointers just in case they are invalid
          GROX_LOG_DEBUG(
              qsession_log, "{:20s} {} {}", client->id(), fmt::ptr(this), "QThread:finished");
          delete client;
          delete thread;
        },
        Qt::DirectConnection);

    // when websocket client finishes, exit the thread
    QObject::connect(
        client_, &qwebsocket_client::finished, client_,
        [this]() {
          GROX_LOG_DEBUG(
              qsession_log, "{:20s} {} invoking thread quit", client_->id(), "Qclient::finished");
          QMetaObject::invokeMethod(this->thread_, "quit", Qt::DirectConnection);
        },
        Qt::DirectConnection);

    // print out when thread is destroyed
    QObject::connect(
        thread_, &QThread::destroyed, thread_,
        [id = client_->id()]() {
          GROX_LOG_DEBUG(qsession_log, "{:20s} {}", id, "QThread::destroyed");
        },
        Qt::DirectConnection);
    thread_->start();
  }

  // ----------------------------------------------------------------------------
  qwebsocket_session::~qwebsocket_session()
  {
    GROX_LOG_INFO(qsession_log, "{:20s} {} Destructor starting", client_->id(), fmt::ptr(this));
    // Stop the connection (queued on the thread)
    client_->stopConnection();
    // Wait for thread to finish cleanly - this ensures the QThread::finished
    // signal has been processed and the cleanup lambda has run
    if (!thread_->wait(5000))
    {
      GROX_LOG_ERROR(qsession_log, "{:20s} {} Destructor: thread_->wait() TIMEOUT!", client_->id(),
          fmt::ptr(this));
    }
    else
    {
      GROX_LOG_INFO(qsession_log, "{:20s} {} Destructor: thread_->wait() completed", client_->id(),
          fmt::ptr(this));
    }
    // DO NOT delete client_ or thread_ here - the QThread::finished signal
    // lambda handles cleanup to avoid double-delete
  }

}    // namespace net::ws
