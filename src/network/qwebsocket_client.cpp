#include <string>
//
#include <fmt/format.h>
//
#include <QtCore/QDebug>
#include <QtCore/QString>
#include <QtNetwork/QAbstractSocket>
#include <QtNetwork/QSslError>
#include <QtWebSockets/QWebSocket>
//
#include "debug/logging.hpp"
#include "network/qwebsocket_client.hpp"

QT_USE_NAMESPACE

template <>
struct fmt::formatter<QString> : formatter<char const*>
{
  auto format(QString const& s, format_context& ctx) const
  {
    return formatter<char const*>::format((char const*) s.toUtf8(), ctx);
  }
};

template <>
struct fmt::formatter<QUrl> : formatter<char const*>
{
  auto format(QUrl const& s, format_context& ctx) const
  {
    return formatter<char const*>::format((char const*) s.toString().toUtf8(), ctx);
  }
};

namespace net::ws {

  // ----------------------------------------------------------------------------
  static auto qwebsocket_log = grox::log::create("QWebsock");

  // ------------------------------------------------------------------
  qwebsocket_client::qwebsocket_client(std::string const& id, QUrl const& url, QString subscribe,
      rx_msg_handler_type handler, QObject* parent)
    : QObject(parent)
    , rx_handler_(handler)
    , url_(url)
    , subscribe_(subscribe)
    , id_(id)
  {
  }

  // ------------------------------------------------------------------
  qwebsocket_client::~qwebsocket_client()
  {
    GROX_LOG_DEBUG(qwebsocket_log, "{:>20} Destructor", id_);
    auto ws = websocket_.load();
    if (ws)
    {
      GROX_LOG_ERROR(
          qwebsocket_log, "{:>20} Client::destructor : websocket delete - out of order", id_);
      delete ws;
      websocket_.store(nullptr);
    }
    else { GROX_LOG_ERROR(qwebsocket_log, "~qwebsocket_client after deletion"); }
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::startConnection()
  {
    GROX_LOG_TRACE(qwebsocket_log, "{:20s} startConnection {}", id_, url_);
    //
    auto ws = new QWebSocket;
    ws->setPauseMode(QAbstractSocket::PauseNever);    // @todo PauseOnSslErrors

    websocket_.store(ws);

    // connection state
    connect(
        ws, &QWebSocket::connected, this, &qwebsocket_client::onConnected, Qt::DirectConnection);
    connect(ws, &QWebSocket::disconnected, this, &qwebsocket_client::onDisconnected,
        Qt::DirectConnection);
    connect(ws, &QWebSocket::stateChanged, this, &qwebsocket_client::onStateChanged,
        Qt::DirectConnection);
    connect(ws, &QWebSocket::aboutToClose, this, &qwebsocket_client::onAboutToClose,
        Qt::DirectConnection);

    // errors
    connect(ws, QOverload<QList<QSslError> const&>::of(&QWebSocket::sslErrors), this,
        &qwebsocket_client::onSslErrors, Qt::DirectConnection);

    connect(ws, SIGNAL(errorOccurred(QAbstractSocket::SocketError)),
        SLOT(onError(QAbstractSocket::SocketError)), Qt::DirectConnection);

    // text messages
    connect(ws, &QWebSocket::textFrameReceived, this, &qwebsocket_client::onTextFrameReceived,
        Qt::DirectConnection);
    // connect(ws, &QWebSocket::textMessageReceived, this,
    //   &qwebsocket_client::onTextMessageReceived, Qt::DirectConnection);
    connect(ws, &QWebSocket::textMessageReceived, this, rx_handler_, Qt::DirectConnection);

    // binary messages
    connect(ws, &QWebSocket::binaryFrameReceived, this, &qwebsocket_client::onBinaryFrameReceived,
        Qt::DirectConnection);
    connect(ws, &QWebSocket::binaryMessageReceived, this,
        &qwebsocket_client::onBinaryMessageReceived, Qt::DirectConnection);

    // others
    connect(ws, &QWebSocket::readChannelFinished, this, &qwebsocket_client::onReadChannelFinished,
        Qt::DirectConnection);
    connect(ws, &QWebSocket::pong, this, &qwebsocket_client::onPong, Qt::DirectConnection);
    connect(ws, &QWebSocket::bytesWritten, this, &qwebsocket_client::onBytesWritten,
        Qt::DirectConnection);

    GROX_LOG_TRACE(qwebsocket_log, "{:20s} openConnection {}", id_, url_);
    QNetworkRequest request = QNetworkRequest(QUrl(url_));
    ws->open(request);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::stopConnection()
  {
    auto ws = websocket_.load();
    if (ws)
    {
      GROX_LOG_DEBUG(qwebsocket_log, "{:20s} stopConnection : invoking WebSocket close", id_);
      // Use QueuedConnection to ensure close happens on the websocket thread
      bool result = QMetaObject::invokeMethod(
          ws, "close", Qt::QueuedConnection, QWebSocketProtocol::CloseCodeNormal);
    }
    else { GROX_LOG_ERROR(qwebsocket_log, "stopConnection after deletion"); }
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onConnected()
  {
    auto ws = websocket_.load();
    if (ws)
    {
      GROX_LOG_TRACE(qwebsocket_log, "{:20s} Connected : sending subscribe", id_);
      ws->sendTextMessage(subscribe_);
    }
    else { GROX_LOG_ERROR(qwebsocket_log, "onConnected after deletion"); }
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onDisconnected()
  {
    auto ws = websocket_.load();
    if (ws)
    {
      GROX_LOG_ERROR(qwebsocket_log, "{:20s} Disconnected : Unexpected : CloseCode is : {} {}", id_,
          QVariant::fromValue(ws->closeCode()).toString(), ws->errorString());
    }
    else { GROX_LOG_ERROR(qwebsocket_log, "onDisconnected after deletion"); }
    emit finished();
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onStateChanged(QAbstractSocket::SocketState socketState)
  {
    GROX_LOG_DEBUG(
        qwebsocket_log, "{:20s} StateChanged {}", id_, QVariant::fromValue(socketState).toString());
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onAboutToClose()
  {
    auto ws = websocket_.load();
    if (ws)
    {
      auto code = ws->closeCode();
      if (code != QWebSocketProtocol::CloseCodeNormal)
      {
        auto reason = ws->closeReason();
        GROX_LOG_ERROR(qwebsocket_log,
            "{:20s} AboutToClose : Unexpected CloseCode is : {} {} : reconnect after time T", id_,
            QVariant::fromValue(code).toString(), ws->errorString());
      }
      else { GROX_LOG_DEBUG(qwebsocket_log, "{:20s} AboutToClose : CloseCode Normal", id_); }
      ws->deleteLater();
      websocket_.store(nullptr);
    }
    else { GROX_LOG_ERROR(qwebsocket_log, "onAboutToClose after deletion"); }
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onSslErrors(QList<QSslError> const& errors)
  {
    for (auto const& err : errors)
    {
      GROX_LOG_ERROR(qwebsocket_log, "{:20s} SslError : {}", id_, err.errorString());
    }
    // qwebsocket_dbg<2>.error(fmt::format("{:20s} SslErrors", id_));
    // Q_UNUSED(errors);

    // WARNING: Never ignore SSL errors in production code.
    // The proper way to handle self-signed certificates is to add a custom root
    // to the CA store.

    auto ws = websocket_.load();
    if (ws) { ws->ignoreSslErrors(); }
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onError(QAbstractSocket::SocketError socketError)
  {
    auto ws = websocket_.load();
    if (ws)
    {
      GROX_LOG_ERROR(qwebsocket_log, "{:20s} SslErrors : Error :{}", id_, ws->errorString());
    }
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onTextFrameReceived(QString const& frame, bool isLastFrame)
  {
    // GROX_LOG_ERROR(qwebsocket_log, "{:20s} TextFrameReceived - this should be overriden", id_);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onTextMessageReceived(QString message)
  {
    GROX_LOG_ERROR(qwebsocket_log, "{:20s} TextMessageReceived - this should be overriden", id_);
    emit processIncomingMessage(message);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onBinaryFrameReceived(QByteArray const& frame, bool isLastFrame)
  {
    GROX_LOG_ERROR(qwebsocket_log, "{:20s} BinaryFrameReceived", id_);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onBinaryMessageReceived(QByteArray const& message)
  {
    GROX_LOG_ERROR(qwebsocket_log, "{:20s} BinaryMessageReceived", id_);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onReadChannelFinished()
  {
    GROX_LOG_DEBUG(qwebsocket_log, "{:20s} ReadChannelFinished", id_);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onPong(quint64 elapsedTime, QByteArray const& payload)
  {
    GROX_LOG_ERROR(qwebsocket_log, "{:20s} Pong", id_);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onBytesWritten(qint64 bytes)
  {
    // GROX_LOG_ERROR(qwebsocket_log, "{:20s} BytesWritten {}", id_, bytes);
  }
}    // namespace net::ws
