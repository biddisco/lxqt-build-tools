#pragma once

#include <functional>
#include <string>
//
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// ----------------------------------------------------------------------------
namespace net::http {

  class qhttp_request_client;
  using client_ptr = qhttp_request_client*;
  using rx_req_handler_type = std::function<void(QByteArray)>;

  class qhttp_request_client : public QObject
  {
    QNetworkAccessManager& networkmanager_;
    QNetworkRequest request_;
    std::string url_;
    std::string content_;
    rx_req_handler_type handler_;
    static std::atomic<int> debug_count_;

public:
    // constructor for url type get
    qhttp_request_client(QNetworkAccessManager& networkmanager,
        std::string const& url /*, rx_req_handler_type&& handler*/);

    // constructor for json type post
    qhttp_request_client(QNetworkAccessManager& networkmanager, std::string const& url,
        std::string&& content /*, rx_req_handler_type&& handler*/);

    // constructor for signed/custom request
    qhttp_request_client(QNetworkAccessManager& networkmanager, QNetworkRequest request,
        std::string&& content /*, rx_req_handler_type&& handler*/);

    ~qhttp_request_client();

    static client_ptr create(QNetworkAccessManager& networkmanager,
        std::string const& url /*, rx_req_handler_type&& handler*/);

    static client_ptr create(QNetworkAccessManager& networkmanager, std::string const& url,
        std::string&& content /*, rx_req_handler_type&& handler*/);

    static client_ptr create_signed(QNetworkAccessManager& networkmanager, QNetworkRequest request,
        std::string&& content /*, rx_req_handler_type&& handler*/);

    void get_request(rx_req_handler_type&& handler);
    void post_request(rx_req_handler_type&& handler);
    void attach_handler(QNetworkReply* reply);

    static void reply_finished(client_ptr self, QNetworkReply* reply);
    static void onSslErrors(client_ptr self, QNetworkReply* reply, QList<QSslError> const& errors);

private:
    void post_request();
    void get_request();
  };

}    // namespace net::http
