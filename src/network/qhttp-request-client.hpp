#pragma once

#include <functional>
#include <string>
//
#include <QNetworkAccessManager>
#include <QNetworkReply>

// ----------------------------------------------------------------------------
namespace net::http {

  class qhttp_request_client;
  using client_ptr = std::shared_ptr<qhttp_request_client>;
  using rx_req_handler_type = std::function<void(client_ptr, std::string_view)>;

  class qhttp_request_client
    : public QObject
    , public std::enable_shared_from_this<qhttp_request_client>
  {
    QNetworkAccessManager& networkmanager_;
    std::string url_;
    std::string content_;
    rx_req_handler_type handler_;
    static std::atomic<int> debug_count_;

public:
    qhttp_request_client(
      QNetworkAccessManager& networkmanager, const std::string& url, rx_req_handler_type&& handler);
    qhttp_request_client(QNetworkAccessManager& networkmanager, const std::string& url,
      std::string&& content, rx_req_handler_type&& handler);
    ~qhttp_request_client();

    static client_ptr create(
      QNetworkAccessManager& networkmanager, const std::string& url, rx_req_handler_type&& handler);

    static client_ptr create(QNetworkAccessManager& networkmanager, const std::string& url,
      std::string&& content, rx_req_handler_type&& handler);

    void get_url_request();
    void post_json_request();

private:
    static void request_finished(client_ptr self, QNetworkReply* reply);
    static void reply_finished(client_ptr self, QNetworkReply* reply);
  };

}    // namespace net::http
