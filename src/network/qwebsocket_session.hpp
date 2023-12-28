#pragma once

#include <memory>
//
#include <QThread>
//
#include "nlohmann/json.hpp"
//
#include "network/qwebsocket_client.hpp"
#include "util/nljson.hpp"

namespace net::ws {

  struct qwebsocket_session
  {
    qwebsocket_client* client_;
    QThread* thread_;

    static std::shared_ptr<qwebsocket_session> create(
      const QString address, const QString subscription, const rx_msg_handler_type handler);

    static std::shared_ptr<qwebsocket_session> create(const std::string address, int port,
      const QString subscription, const rx_msg_handler_type handler);

    static std::shared_ptr<qwebsocket_session> create(const std::string address, int port,
      util::string_t subscription, const rx_msg_handler_type handler);

    qwebsocket_session(
      const QString address, const QString subscription, const rx_msg_handler_type handler);

    ~qwebsocket_session();
  };

}    // namespace net::ws
