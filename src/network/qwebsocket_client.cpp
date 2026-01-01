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
#include "debug/print.hpp"
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
  using namespace grox::debug::detail;
  template <int Level>
  inline constexpr print_threshold<Level, 2> qwebsocket_dbg("QWebsock");

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
    qwebsocket_dbg<2>.debug(ffmt<s20>(id_), "Destructor");
    if (websocket_)
    {
      qwebsocket_dbg<1>.error(
          ffmt<s20>(id_), "Client::destructor : websocket delete - out of order");
      delete websocket_;
    }
    else { qwebsocket_dbg<3>.error("~qwebsocket_client after deletion"); }
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::startConnection()
  {
    qwebsocket_dbg<2>.debug(fmt::format("{:20s} startConnection", id_, url_));
    //
    websocket_ = new QWebSocket;
    (*websocket_).setPauseMode(QAbstractSocket::PauseNever);    // @todo PauseOnSslErrors

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
    connect(websocket_, QOverload<QList<QSslError> const&>::of(&QWebSocket::sslErrors), this,
        &qwebsocket_client::onSslErrors, Qt::DirectConnection);

    connect(websocket_, SIGNAL(errorOccurred(QAbstractSocket::SocketError)),
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

    qwebsocket_dbg<2>.debug(fmt::format("{:20s} openConnection {}", id_, url_));
    QNetworkRequest request = QNetworkRequest(QUrl(url_));
    (*websocket_).open(request);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::stopConnection()
  {
    if (websocket_)
    {
      qwebsocket_dbg<2>.debug(fmt::format("{:20s} stopConnection : invoking WebSocket close", id_));
      bool result = QMetaObject::invokeMethod(
          websocket_, "close", Qt::QueuedConnection, QWebSocketProtocol::CloseCodeNormal);
    }
    else { qwebsocket_dbg<3>.error("stopConnection after deletion"); }
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onConnected()
  {
    qwebsocket_dbg<3>.debug(fmt::format("{:20s} Connected : sending subscribe", id_));
    (*websocket_).sendTextMessage(subscribe_);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onDisconnected()
  {
    if (websocket_)
    {
      qwebsocket_dbg<2>.error(fmt::format("{:20s} Disconnected : Unexpected : CloseCode is : {} {}",
          id_, QVariant::fromValue((*websocket_).closeCode()).toString(),
          (*websocket_).errorString()));
    }
    else { qwebsocket_dbg<3>.error("onDisconnected after deletion"); }
    emit finished();
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onStateChanged(QAbstractSocket::SocketState socketState)
  {
    qwebsocket_dbg<3>.debug(
        fmt::format("{:20s} StateChanged {}", id_, QVariant::fromValue(socketState).toString()));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onAboutToClose()
  {
    if (websocket_)
    {
      auto code = (*websocket_).closeCode();
      if (code != QWebSocketProtocol::CloseCodeNormal)
      {
        auto reason = (*websocket_).closeReason();
        qwebsocket_dbg<2>.error(fmt::format(
            "{:20s} AboutToClose : Unexpected CloseCode is : {} {} : reconnect after time T", id_,
            QVariant::fromValue(code).toString(), (*websocket_).errorString()));
      }
      else { qwebsocket_dbg<3>.debug(fmt::format("{:20s} AboutToClose : CloseCode Normal", id_)); }
      (*websocket_).deleteLater();
      websocket_ = nullptr;
    }
    else { qwebsocket_dbg<3>.error("onAboutToClose after deletion"); }
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onSslErrors(QList<QSslError> const& errors)
  {
    for (auto const& err : errors)
    {
      qwebsocket_dbg<2>.error(fmt::format("{:20s} SslError : {}", id_, err.errorString()));
    }
    // qwebsocket_dbg<2>.error(fmt::format("{:20s} SslErrors", id_));
    // Q_UNUSED(errors);

    // WARNING: Never ignore SSL errors in production code.
    // The proper way to handle self-signed certificates is to add a custom root
    // to the CA store.

    (*websocket_).ignoreSslErrors();
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onError(QAbstractSocket::SocketError socketError)
  {
    qwebsocket_dbg<2>.error(
        fmt::format("{:20s} SslErrors : Error :{}", id_, (*websocket_).errorString()));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onTextFrameReceived(QString const& frame, bool isLastFrame)
  {
    qwebsocket_dbg<9>.error(
        fmt::format("{:20s} TextFrameReceived - this should be overriden", id_));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onTextMessageReceived(QString message)
  {
    qwebsocket_dbg<2>.error(
        fmt::format("{:20s} TextMessageReceived - this should be overriden", id_));
    emit processIncomingMessage(message);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onBinaryFrameReceived(QByteArray const& frame, bool isLastFrame)
  {
    qwebsocket_dbg<5>.error(fmt::format("{:20s} BinaryFrameReceived", id_));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onBinaryMessageReceived(QByteArray const& message)
  {
    qwebsocket_dbg<5>.error(fmt::format("{:20s} BinaryMessageReceived", id_));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onReadChannelFinished()
  {
    qwebsocket_dbg<3>.debug(fmt::format("{:20s} ReadChannelFinished", id_));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onPong(quint64 elapsedTime, QByteArray const& payload)
  {
    qwebsocket_dbg<9>.error(fmt::format("{:20s} Pong", id_));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onBytesWritten(qint64 bytes)
  {
    qwebsocket_dbg<9>.error(fmt::format("{:20s} BytesWritten {}", id_, bytes));
  }
}    // namespace net::ws
