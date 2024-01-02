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
  qhttp_request_client::qhttp_request_client(
    QNetworkAccessManager& networkmanager, const std::string& url, rx_req_handler_type&& handler)
    : networkmanager_(networkmanager)
    , url_(url)
    , handler_(handler)
  {
    debug_count_++;
    /* We do not use this as it triggers  callback on every client, for every finished
     * so we cannot easily tell which reply belongs to which request
      connect(&networkmanager_, &QNetworkAccessManager::finished, this,
        std::bind(&qhttp_request_client::request_finished, this, _1),
        Qt::AutoConnection);
    */
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
  }

  // ----------------------------------------------------------------------------
  qhttp_request_client::~qhttp_request_client()
  {
    // just for debugging, to track use
    debug_count_--;
    http_dbg<5>.debug(str<>("destructor"), debug_count_.load());
  }

  // ----------------------------------------------------------------------------
  void qhttp_request_client::get_url_request()
  {
    QNetworkRequest request(QUrl(url_.c_str()));
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
    request.setRawHeader("User-Agent", "mystery");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Connection", "close");

    // issue get request
    http_dbg<2>.debug(str<>("get_url_request"), url_);
    QNetworkReply* reply = networkmanager_.get(request);
    if (nullptr != reply)
    {
      if (reply->isRunning())
      {
        connect(reply, &QNetworkReply::finished, this,
          std::bind(&qhttp_request_client::reply_finished, shared_from_this(), reply),
          Qt::AutoConnection);
        connect(reply, &QNetworkReply::errorOccurred, this,
          [](QNetworkReply::NetworkError err) { qDebug() << QVariant(err).toString(); });
      }
      else
      {    // if already finished
        reply_finished(shared_from_this(), reply);
      }
    }
  }

  // ----------------------------------------------------------------------------
  void qhttp_request_client::post_json_request()
  {
    QNetworkRequest request(QUrl(url_.c_str()));
    request.setRawHeader("Content-Type", "application/json");
    request.setRawHeader("User-Agent", "mystery");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Connection", "close");

    // issue post request
    http_dbg<2>.debug(str<>("post_json_request"), url_, content_);
    QNetworkReply* reply = networkmanager_.post(request, QByteArray(content_.data()));
    if (nullptr != reply)
    {
      if (reply->isRunning())
      {
        connect(reply, &QNetworkReply::finished, this,
          std::bind(&qhttp_request_client::reply_finished, shared_from_this(), reply),
          Qt::AutoConnection);
        connect(reply, &QNetworkReply::errorOccurred, this,
          [](QNetworkReply::NetworkError err) { qDebug() << QVariant(err).toString(); });
      }
      else
      {    // if already finished
        reply_finished(shared_from_this(), reply);
      }
    }
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
}    // namespace net::http
