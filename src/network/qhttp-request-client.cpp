#include <functional>
#include <iostream>
//
#include <QNetworkAccessManager>
#include <QNetworkReply>
//
#include "debug/print.hpp"
#include "network/qhttp-request-client.hpp"

// ----------------------------------------------------------------------------
using namespace grox::debug;
//
template <int Level>
static print_threshold<Level, 2> http_dbg("https://");

namespace net::http {

  using namespace std::placeholders;

  std::atomic<int> qhttp_request_client::debug_count_ = 0;

  // ----------------------------------------------------------------------------
  client_ptr qhttp_request_client::create(
    QNetworkAccessManager& networkmanager, const std::string& url, rx_req_handler_type&& handler)
  {
    return std::make_shared<qhttp_request_client>(
      networkmanager, url, std::forward<rx_req_handler_type>(handler));
  }

  // ----------------------------------------------------------------------------
  client_ptr qhttp_request_client::create(QNetworkAccessManager& networkmanager,
    const std::string& url, std::string&& content, rx_req_handler_type&& handler)
  {
    return std::make_shared<qhttp_request_client>(networkmanager, url,
      std::forward<std::string>(content), std::forward<rx_req_handler_type>(handler));
  }

  // ----------------------------------------------------------------------------
  client_ptr qhttp_request_client::create_signed(QNetworkAccessManager& networkmanager,
    QNetworkRequest request, std::string&& content, rx_req_handler_type&& handler)
  {
    return std::make_shared<qhttp_request_client>(
      networkmanager, request, std::move(content), std::forward<rx_req_handler_type>(handler));
  }

  // ----------------------------------------------------------------------------
  qhttp_request_client::qhttp_request_client(
    QNetworkAccessManager& networkmanager, const std::string& url, rx_req_handler_type&& handler)
    : networkmanager_(networkmanager)
    , url_(url)
    , handler_(handler)
  {
    debug_count_++;
    request_.setUrl(QUrl(url_.c_str()));
    request_.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
    request_.setRawHeader("User-Agent", "mystery");
    request_.setRawHeader("Accept", "application/json");
    request_.setRawHeader("Connection", "close");
  }

  // ----------------------------------------------------------------------------
  qhttp_request_client::qhttp_request_client(QNetworkAccessManager& networkmanager,
    const std::string& url, std::string&& content, rx_req_handler_type&& handler)
    : networkmanager_(networkmanager)
    , url_(url)
    , content_(std::move(content))
    , handler_(handler)
  {
    debug_count_++;
    request_.setUrl(QUrl(url_.c_str()));
    request_.setRawHeader("Content-Type", "application/json");
    request_.setRawHeader("User-Agent", "mystery");
    request_.setRawHeader("Accept", "application/json");
    request_.setRawHeader("Connection", "close");
  }

  // ----------------------------------------------------------------------------
  qhttp_request_client::qhttp_request_client(QNetworkAccessManager& networkmanager,
    QNetworkRequest request, std::string&& content, rx_req_handler_type&& handler)
    : networkmanager_(networkmanager)
    , request_(request)
    , content_(std::move(content))
    , handler_(handler)
  {
    debug_count_++;
  }

  // ----------------------------------------------------------------------------
  qhttp_request_client::~qhttp_request_client()
  {
    // just for debugging, to track use
    debug_count_--;
    http_dbg<5>.debug(str<>("destructor"), debug_count_.load());
  }

  // ----------------------------------------------------------------------------
  inline void attach_handler(qhttp_request_client* self, QNetworkReply* reply)
  {
    if (nullptr != reply)
    {
      if (reply->isRunning())
      {
        self->connect(reply, &QNetworkReply::finished, self,
          std::bind(&qhttp_request_client::reply_finished, self->shared_from_this(), reply),
          Qt::AutoConnection);
        self->connect(
          reply, &QNetworkReply::errorOccurred, self,
          [](QNetworkReply::NetworkError err) {
            qDebug() << QVariant(err).toString();
            // std::terminate();
          },
          Qt::DirectConnection);
        self->connect(reply, &QNetworkReply::sslErrors, self, &qhttp_request_client::onSslErrors,
          Qt::DirectConnection);
      }
      else
      {    // if already finished
        self->reply_finished(self->shared_from_this(), reply);
      }
    }
  }

  // ----------------------------------------------------------------------------
  void qhttp_request_client::get_request()
  {
    // issue get request
    http_dbg<2>.debug(str<>("get_request"), url_);
    QNetworkReply* reply = networkmanager_.get(request_);
    attach_handler(this, reply);
  }

  // ----------------------------------------------------------------------------
  void qhttp_request_client::post_request()
  {
    // issue post request
    http_dbg<2>.debug(str<>("post_request"), url_, content_);
    QNetworkReply* reply = networkmanager_.post(request_, QByteArray(content_.data()));
    attach_handler(this, reply);
  }

  // ----------------------------------------------------------------------------
  void qhttp_request_client::request_finished(client_ptr self, QNetworkReply* reply)
  {
    std::cerr << "request_finished" << std::endl;
  }

  // ----------------------------------------------------------------------------
  void qhttp_request_client::reply_finished(client_ptr self, QNetworkReply* reply)
  {
    if (!reply)
      return;
    // convert raw data into std::string, this should be safe since our http traffic is utf8
    QByteArray byteArray = reply->readAll();
    std::string_view str(byteArray.constData(), byteArray.length());
    // invoke handler with result data in string form
    self->handler_(self, str);
    //
    delete reply;
  }

  // ----------------------------------------------------------------------------
  void qhttp_request_client::onSslErrors(const QList<QSslError>& errors)
  {
    QString errorString;
    foreach (const QSslError& error, errors)
    {
      if (!errorString.isEmpty())
        errorString += '\n';
      errorString += error.errorString();
    }

    qDebug() << "SSL Error: " << errorString;
  }
}    // namespace net::http
