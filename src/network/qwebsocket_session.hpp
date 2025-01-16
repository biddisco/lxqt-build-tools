#pragma once

#include <memory>
#include <string>
//
#include "nlohmann/json.hpp"
//
#include <QThread>
//
#include "network/qwebsocket_client.hpp"
#include "util/json_qstring.hpp"

namespace net::ws {

  struct qwebsocket_session
  {
    qwebsocket_client* client_;
    QThread* thread_;

    static std::shared_ptr<qwebsocket_session> create(std::string const& id, QString const address,
        QString const subscription, rx_msg_handler_type const handler);

    static std::shared_ptr<qwebsocket_session> create(std::string const& id,
        std::string const& address, int port, QString const subscription,
        rx_msg_handler_type const handler);

    static std::shared_ptr<qwebsocket_session> create(std::string const& id,
        std::string const& address, int port, util::string_t subscription,
        rx_msg_handler_type const handler);

    qwebsocket_session(std::string const& id, QString const address, QString const subscription,
        rx_msg_handler_type const handler);

    ~qwebsocket_session();
  };

}    // namespace net::ws
