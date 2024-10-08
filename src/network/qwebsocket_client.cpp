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
  static print_threshold<Level, 5> qwebsocket_dbg("QWebsock");

  // ------------------------------------------------------------------
  qwebsocket_client::qwebsocket_client(const std::string& id, const QUrl& url, QString subscribe,
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
    qwebsocket_dbg<2>.debug(fmt::format("{:20s} Destructor", id_));
    if (websocket_)
    {
      qwebsocket_dbg<2>.error(
          fmt::format("{:20s}, Client::destructor : websocket delete - out of order", id_));
      delete websocket_;
    }
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::startConnection()
  {
    qwebsocket_dbg<2>.debug(fmt::format("{:20s} startConnection", id_, url_));
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

    qwebsocket_dbg<2>.debug(fmt::format("{:20s} openConnection {}", id_, url_));
    QNetworkRequest request = QNetworkRequest(QUrl(url_));
    websocket_->open(request);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::stopConnection()
  {
    if (websocket_ != nullptr)
    {
      qwebsocket_dbg<2>.debug(fmt::format("{:20s} stopConnection : invoking WebSocket close", id_));
      QMetaObject::invokeMethod(websocket_, "close", Qt::QueuedConnection);
      websocket_->deleteLater();
      websocket_ = nullptr;
    }
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onConnected()
  {
    qwebsocket_dbg<2>.debug(fmt::format("{:20s} Connected : sending subscribe", id_));
    websocket_->sendTextMessage(subscribe_);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onDisconnected()
  {
    if (websocket_)
    {
      qwebsocket_dbg<2>.error(fmt::format("{:20s} Disconnected : Unexpected : CloseCode is : {} {}",
          id_, QVariant::fromValue(websocket_->closeCode()).toString(), websocket_->errorString()));
    }
    emit finished();
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onStateChanged(QAbstractSocket::SocketState socketState)
  {
    qwebsocket_dbg<2>.debug(
        fmt::format("{:20s} StateChanged {}", id_, QVariant::fromValue(socketState).toString()));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onAboutToClose()
  {
    if (websocket_)
    {
      auto reason = websocket_->closeReason();
      qwebsocket_dbg<0>.error(fmt::format(
          "{:20s} AboutToClose : Unexpected CloseCode is : {} {} : reconnect after time T", id_,
          QVariant::fromValue(websocket_->closeCode()).toString(), websocket_->errorString()));
    }
    /*setTimeout(setupWebSocket, 1000);*/
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onSslErrors(const QList<QSslError>& errors)
  {
    qwebsocket_dbg<0>.error(fmt::format("{:20s} SslErrors", id_));
    Q_UNUSED(errors);

    // WARNING: Never ignore SSL errors in production code.
    // The proper way to handle self-signed certificates is to add a custom root
    // to the CA store.

    websocket_->ignoreSslErrors();
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onError(QAbstractSocket::SocketError socketError)
  {
    qwebsocket_dbg<0>.error(
        fmt::format("{:20s} SslErrors : Error :{}", id_, websocket_->errorString()));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onTextFrameReceived(const QString& frame, bool isLastFrame)
  {
    qwebsocket_dbg<9>.error(
        fmt::format("{:20s} TextFrameReceived - this should be overriden", id_));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onTextMessageReceived(QString message)
  {
    qwebsocket_dbg<0>.error(
        fmt::format("{:20s} TextMessageReceived - this should be overriden", id_));
    emit processIncomingMessage(message);
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onBinaryFrameReceived(const QByteArray& frame, bool isLastFrame)
  {
    qwebsocket_dbg<5>.error(fmt::format("{:20s} BinaryFrameReceived", id_));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onBinaryMessageReceived(const QByteArray& message)
  {
    qwebsocket_dbg<5>.error(fmt::format("{:20s} BinaryMessageReceived", id_));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onReadChannelFinished()
  {
    qwebsocket_dbg<3>.debug(fmt::format("{:20s} ReadChannelFinished", id_));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onPong(quint64 elapsedTime, const QByteArray& payload)
  {
    qwebsocket_dbg<9>.error(fmt::format("{:20s} Pong", id_));
  }

  // ------------------------------------------------------------------
  void qwebsocket_client::onBytesWritten(qint64 bytes)
  {
    qwebsocket_dbg<9>.error(fmt::format("{:20s} BytesWritten {}", id_, bytes));
  }
}    // namespace net::ws
