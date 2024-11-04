#pragma once

#include <memory>
//
#include <QThread>
//
#include "nlohmann/json.hpp"
//
#include "network/qwebsocket_client.hpp"
#include "util/json_qstring.hpp"

namespace net::ws {

  struct qwebsocket_session
  {
    qwebsocket_client* client_;
    QThread* thread_;

    static std::shared_ptr<qwebsocket_session> create(std::string const& id, const QString address,
        const QString subscription, const rx_msg_handler_type handler);

    static std::shared_ptr<qwebsocket_session> create(std::string const& id,
        std::string const& address, int port, const QString subscription,
        const rx_msg_handler_type handler);

    static std::shared_ptr<qwebsocket_session> create(std::string const& id,
        std::string const& address, int port, util::string_t subscription,
        const rx_msg_handler_type handler);

    qwebsocket_session(std::string const& id, const QString address, const QString subscription,
        const rx_msg_handler_type handler);

    ~qwebsocket_session();
  };

}    // namespace net::ws
