#include <atomic>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
//
#include <QNetworkAccessManager>
#include <QNetworkReply>
//
#include "debug/logging.hpp"
#include "network/qhttp-request-client.hpp"
#include "util/stringutils.hpp"

// ----------------------------------------------------------------------------
static auto http_log = grox::log::create("https://");

// ----------------------------------------------------------------------------
namespace net::http {

  std::atomic<int> qhttp_request_client::debug_count_ = 0;

  // ----------------------------------------------------------------------------
  client_ptr qhttp_request_client::create(
      QNetworkAccessManager& networkmanager, std::string const& url)
  {
    //return std::make_shared<qhttp_request_client>(networkmanager, url, move(handler));
    return new qhttp_request_client(networkmanager, url /*, move(handler)*/);
  }

  // ----------------------------------------------------------------------------
  client_ptr qhttp_request_client::create(
      QNetworkAccessManager& networkmanager, std::string const& url, std::string&& content)
  {
    // return std::make_shared<qhttp_request_client>(
    //   networkmanager, url, std::forward<std::string>(content), std::move(handler));
    return new qhttp_request_client(
        networkmanager, url, std::forward<std::string>(content) /*, std::move(handler)*/);
  }

  // ----------------------------------------------------------------------------
  client_ptr qhttp_request_client::create_signed(
      QNetworkAccessManager& networkmanager, QNetworkRequest request, std::string&& content)
  {
    return new qhttp_request_client(
        networkmanager, request, std::move(content) /*, std::move(handler)*/);
    // return std::make_shared<qhttp_request_client>(
    //   networkmanager, request, std::move(content), std::move(handler));
  }

  // ----------------------------------------------------------------------------
  qhttp_request_client::qhttp_request_client(
      QNetworkAccessManager& networkmanager, std::string const& url)
    : networkmanager_(networkmanager)
    , url_(url)
  {
    debug_count_++;
    request_.setUrl(QUrl(to_qstring(url_)));
    request_.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
    request_.setRawHeader("User-Agent", "mystery");
    request_.setRawHeader("Accept", "application/json");
    request_.setRawHeader("Connection", "close");
  }

  // ----------------------------------------------------------------------------
  qhttp_request_client::qhttp_request_client(
      QNetworkAccessManager& networkmanager, std::string const& url, std::string&& content)
    : networkmanager_(networkmanager)
    , url_(url)
    , content_(std::move(content))
  {
    debug_count_++;
    request_.setUrl(QUrl(to_qstring(url_)));
    request_.setRawHeader("Content-Type", "application/json");
    request_.setRawHeader("User-Agent", "mystery");
    request_.setRawHeader("Accept", "application/json");
    request_.setRawHeader("Connection", "close");
  }

  // ----------------------------------------------------------------------------
  qhttp_request_client::qhttp_request_client(
      QNetworkAccessManager& networkmanager, QNetworkRequest request, std::string&& content)
    : networkmanager_(networkmanager)
    , request_(request)
    , content_(std::move(content))
  {
    debug_count_++;
  }

  // ----------------------------------------------------------------------------
  qhttp_request_client::~qhttp_request_client()
  {
    // just for debugging, to track use
    debug_count_--;
    GROX_LOG_DEBUG(http_log, "{:>20} {} {}", "destructor", fmt::ptr(this), debug_count_.load());
  }

  // ----------------------------------------------------------------------------
  void qhttp_request_client::attach_handler(QNetworkReply* reply)
  {
    using namespace std::placeholders;
    if (nullptr == reply)
    {
      GROX_LOG_ERROR(http_log, "{:>20} {} fail : nullptr", "attach_handler", fmt::ptr(this));
      return;
    }
    if (reply->isRunning())
    {
      QObject::connect(reply, &QNetworkReply::finished, this,
          std::bind(&qhttp_request_client::reply_finished, this, reply), Qt::DirectConnection);

      QObject::connect(
          reply, &QNetworkReply::errorOccurred, this,
          [this, reply](QNetworkReply::NetworkError err) {
            GROX_LOG_ERROR(http_log, "{:>20} {} {} {}", "handler", fmt::ptr(this),
                QVariant(err).toString().toStdString(), reply->errorString().toStdString());
          },
          Qt::DirectConnection);

      QObject::connect(reply, &QNetworkReply::sslErrors, this,
          std::bind(&qhttp_request_client::onSslErrors, this, reply, _1), Qt::DirectConnection);
    }
    else
    {    // if already finished
      GROX_LOG_DEBUG(http_log, "{:>20} {} attach handler", "early completion", fmt::ptr(this));
      reply_finished(this, reply);
    }
  }

  // ----------------------------------------------------------------------------
  void qhttp_request_client::get_request()
  {
    // issue get request
    GROX_LOG_DEBUG(http_log, "{:>20} {} {}", "get_request", fmt::ptr(this), url_);
    QNetworkReply* reply = networkmanager_.get(request_);
    attach_handler(reply);
  }

  // ----------------------------------------------------------------------------
  void qhttp_request_client::get_request(rx_req_handler_type&& handler)
  {
    handler_ = std::move(handler);
    get_request();
  }

  // ----------------------------------------------------------------------------
  void qhttp_request_client::post_request()
  {
    // issue post request
    GROX_LOG_DEBUG(http_log, "{:>20} {} {} {}", "post_request", fmt::ptr(this), url_, content_);
    QNetworkReply* reply = networkmanager_.post(request_, QByteArray(content_.data()));
    attach_handler(reply);
  }

  // ----------------------------------------------------------------------------
  void qhttp_request_client::post_request(rx_req_handler_type&& handler)
  {
    handler_ = std::move(handler);
    post_request();
  }

  // ----------------------------------------------------------------------------
  void qhttp_request_client::reply_finished(client_ptr self, QNetworkReply* reply)
  {
    GROX_LOG_DEBUG(http_log, "{:>20} {}", "reply_finished", fmt::ptr(self));
    if (!reply) return;
    // convert raw data into std::string, this should be safe since our http traffic is utf8
    QByteArray byteArray = reply->readAll();
    std::string_view str(byteArray.constData(), byteArray.length());
    // invoke handler with result data moved in
    self->handler_(std::move(byteArray));
    //
    delete reply;
    delete self;
  }

  // ----------------------------------------------------------------------------
  void qhttp_request_client::onSslErrors(
      client_ptr self, QNetworkReply* reply, QList<QSslError> const& errors)
  {
    reply->ignoreSslErrors();
    return;

    // http_dbg<0>.debug(ffmt<s20>("onSslErrors"), self);
    // QString errorString;
    // foreach (QSslError const& error, errors)
    // {
    //   if (!errorString.isEmpty())
    //     errorString += '\n';
    //   errorString += error.errorString();
    // }
    // qDebug() << "SSL Error: " << errorString;
    // delete reply;
    // delete self;
  }

}    // namespace net::http
