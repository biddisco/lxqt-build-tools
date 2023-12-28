#include <QtCore/QDebug>
#include <QtCore/QString>
#include <QtNetwork/QAbstractSocket>
#include <QtNetwork/QSslError>
#include <QtWebSockets/QWebSocket>
//
#include <fmt/format.h>
//
#include "debug/print.hpp"
#include "network/qwebsocket_client.hpp"

QT_USE_NAMESPACE

template <>
struct fmt::formatter<QString> : formatter<const char*>
{
  auto format(const QString& s, format_context& ctx)
  {
    return formatter<const char*>::format((const char*) s.toUtf8(), ctx);
  }
};

template <>
struct fmt::formatter<QUrl> : formatter<const char*>
{
  auto format(const QUrl& s, format_context& ctx)
  {
    return formatter<const char*>::format((const char*) s.toString().toUtf8(), ctx);
  }
};

namespace net::ws {

  // ----------------------------------------------------------------------------
  using namespace grox::debug;
  template <int Level>
  static print_threshold<Level, 5> qweb_dbg("QWebsock");

  // ------------------------------------------------------------------
  qwebsocket_client::qwebsocket_client(
    const QUrl& url, QString subscribe, rx_msg_handler_type handler, QObject* parent)
    : QObject(parent)
  {
    rx_handler_ = handler;
    url_ = url;
    subscribe_ = subscribe;
  }

  // ------------------------------------------------------------------
  qwebsocket_client::~qwebsocket_client()
  {
    qweb_dbg<2>.debug(fmt::format("{:17s} Destructor", "Client::WebSocket"));
    if (websocket_)
    {
      qweb_dbg<2>.error(
        fmt::format("{:17s}, {:s}", "Client::destructor", "websocket delete - out of order"));
      delete websocket_;
    }
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::startConnection()
  {
    qweb_dbg<2>.debug(fmt::format("{:17s} startConnection", "Client::WebSocket", url_));
    //
    websocket_ = new QWebSocket;
    websocket_->setPauseMode(QAbstractSocket::PauseNever);    // @todo PauseOnSslErrors

    // connection state
    connect(websocket_, &QWebSocket::connected, this, &qwebsocket_client::onConnected,
      Qt::DirectConnection);
    connect(websocket_, &QWebSocket::disconnected, this, &qwebsocket_client::onDisconnected),
      Qt::DirectConnection;
    connect(websocket_, &QWebSocket::stateChanged, this, &qwebsocket_client::onStateChanged,
      Qt::DirectConnection);
    connect(websocket_, &QWebSocket::aboutToClose, this, &qwebsocket_client::onAboutToClose,
      Qt::DirectConnection);

    // errors
    connect(websocket_, QOverload<const QList<QSslError>&>::of(&QWebSocket::sslErrors), this,
      &qwebsocket_client::onSslErrors, Qt::DirectConnection);
    connect(websocket_, SIGNAL(error(QAbstractSocket::SocketError)),
      SLOT(onError(QAbstractSocket::SocketError)), Qt::DirectConnection);

    // text messages
    connect(websocket_, &QWebSocket::textFrameReceived, this,
      &qwebsocket_client::onTextFrameReceived, Qt::DirectConnection);
    // connect(websocket_, &QWebSocket::textMessageReceived, this,
    //   &qwebsocket_client::onTextMessageReceived, Qt::DirectConnection);
    connect(websocket_, &QWebSocket::textMessageReceived, this, rx_handler_, Qt::DirectConnection);

    // binary messages
    connect(websocket_, &QWebSocket::binaryFrameReceived, this,
      &qwebsocket_client::onBinaryFrameReceived, Qt::DirectConnection);
    connect(websocket_, &QWebSocket::binaryMessageReceived, this,
      &qwebsocket_client::onBinaryMessageReceived, Qt::DirectConnection);

    // others
    connect(websocket_, &QWebSocket::readChannelFinished, this,
      &qwebsocket_client::onReadChannelFinished, Qt::DirectConnection);
    connect(websocket_, &QWebSocket::pong, this, &qwebsocket_client::onPong, Qt::DirectConnection);
    connect(websocket_, &QWebSocket::bytesWritten, this, &qwebsocket_client::onBytesWritten,
      Qt::DirectConnection);

    qweb_dbg<2>.debug(fmt::format("{:17s} openConnection {:s}", "Client::WebSocket", url_));
    QNetworkRequest request = QNetworkRequest(QUrl(url_));
    websocket_->open(request);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::stopConnection()
  {
    if (websocket_ != nullptr)
    {
      qweb_dbg<2>.debug(
        fmt::format("{:17s} stopConnection : invoking WebSocket close", "Client::WebSocket"));
      QMetaObject::invokeMethod(websocket_, "close", Qt::QueuedConnection);
      websocket_->deleteLater();
      websocket_ = nullptr;
    }
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onConnected()
  {
    qweb_dbg<2>.debug(fmt::format("{:17s} Connected : sending subscribe", "Client::WebSocket"));
    websocket_->sendTextMessage(subscribe_);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onDisconnected()
  {
    if (websocket_)
    {
      qweb_dbg<2>.error(fmt::format("{:17s} Disconnected : Unexpected : CloseCode is : {} {:s}",
        "Client::WebSocket", QVariant::fromValue(websocket_->closeCode()).toString(),
        websocket_->errorString()));
    }
    emit finished();
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onStateChanged(QAbstractSocket::SocketState socketState)
  {
    qweb_dbg<2>.debug(fmt::format(
      "{:17s} StateChanged {}", "Client::WebSocket", QVariant::fromValue(socketState).toString()));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onAboutToClose()
  {
    if (websocket_)
    {
      auto reason = websocket_->closeReason();
      qweb_dbg<0>.error(fmt::format(
        "{:17s} AboutToClose : Unexpected CloseCode is : {} {:s} : reconnect after time T",
        "Client::WebSocket", QVariant::fromValue(websocket_->closeCode()).toString(),
        websocket_->errorString()));
    }
    /*setTimeout(setupWebSocket, 1000);*/
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onSslErrors(const QList<QSslError>& errors)
  {
    qweb_dbg<0>.error(fmt::format("{:17s} SslErrors", "Client::WebSocket"));
    Q_UNUSED(errors);

    // WARNING: Never ignore SSL errors in production code.
    // The proper way to handle self-signed certificates is to add a custom root
    // to the CA store.

    websocket_->ignoreSslErrors();
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onError(QAbstractSocket::SocketError socketError)
  {
    qweb_dbg<0>.error(
      fmt::format("{:17s} SslErrors : Error :{}", "Client::WebSocket", websocket_->errorString()));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onTextFrameReceived(const QString& frame, bool isLastFrame)
  {
    qweb_dbg<9>.error(
      fmt::format("{:17s} TextFrameReceived - this should be overriden", "Client::WebSocket"));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onTextMessageReceived(QString message)
  {
    qweb_dbg<2>.error(
      fmt::format("{:17s} TextMessageReceived - this should be overriden", "Client::WebSocket"));
    emit processIncomingMessage(message);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onBinaryFrameReceived(const QByteArray& frame, bool isLastFrame)
  {
    qweb_dbg<5>.error(fmt::format("{:17s} BinaryFrameReceived", "Client::WebSocket"));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onBinaryMessageReceived(const QByteArray& message)
  {
    qweb_dbg<5>.error(fmt::format("{:17s} BinaryMessageReceived", "Client::WebSocket"));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onReadChannelFinished()
  {
    qweb_dbg<3>.error(fmt::format("{:17s} ReadChannelFinished", "Client::WebSocket"));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onPong(quint64 elapsedTime, const QByteArray& payload)
  {
    qweb_dbg<9>.error(fmt::format("{:17s} Pong", "Client::WebSocket"));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onBytesWritten(qint64 bytes)
  {
    qweb_dbg<9>.error(fmt::format("{:17s} BytesWritten {}", "Client::WebSocket", bytes));
  }
}    // namespace net::ws
