#include <QtCore/QDebug>
#include <QtNetwork/QAbstractSocket>
#include <QtNetwork/QSslError>
#include <QtWebSockets/QWebSocket>
//
#include "qwebsocket_client.hpp"

QT_USE_NAMESPACE

// ------------------------------------------------------------------
qwebsocket_client::qwebsocket_client(
  const QUrl& url, QString subscribe, msg_received_type handler, QObject* parent)
  : QObject(parent)
{
  rx_handler_ = handler;
  url_ = url;
  subscribe_ = subscribe;
}

// ------------------------------------------------------------------
qwebsocket_client::~qwebsocket_client()
{
  qDebug() << "Client::destructor";
  if (websocket_)
  {
    qDebug() << "Client::destructor : websocket not yet deleted";
    delete websocket_;
  }
}

// ------------------------------------------------------------------
void qwebsocket_client::startConnection()
{
  qDebug() << "Client::startConnection:" << url_;
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
  connect(websocket_, &QWebSocket::textFrameReceived, this, &qwebsocket_client::onTextFrameReceived,
    Qt::DirectConnection);
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

  qDebug() << "Client::connection opening:" << url_;
  QNetworkRequest request = QNetworkRequest(QUrl(url_));
  websocket_->open(request);
}

// ------------------------------------------------------------------
void qwebsocket_client::stopConnection()
{
  if (websocket_ != nullptr)
  {
    qDebug() << "Client::stopConnection invoking WebSocket close";
    QMetaObject::invokeMethod(websocket_, "close", Qt::QueuedConnection);
    websocket_->deleteLater();
    websocket_ = nullptr;
  }
}

// ------------------------------------------------------------------
void qwebsocket_client::onConnected()
{
  qDebug() << "Client::WebSocket connected";
  websocket_->sendTextMessage(subscribe_);
}

// ------------------------------------------------------------------
void qwebsocket_client::onDisconnected()
{
  if (websocket_)
  {
    qDebug() << "Client::WebSocket Unexpected disconnected"
             << " CloseCode is : " << websocket_->closeCode() << " " << websocket_->errorString();
  }
  qDebug() << "Client::Websocket disconnected : emitting finished";
  emit finished();
}

// ------------------------------------------------------------------
void qwebsocket_client::onStateChanged(QAbstractSocket::SocketState socketState)
{
  qDebug() << "Client::WebSocket StateChanged : " << socketState;
}

// ------------------------------------------------------------------
void qwebsocket_client::onAboutToClose()
{
  if (websocket_)
  {
    auto reason = websocket_->closeReason();
    qDebug() << "Client::WebSocket Unexpected AboutToClose"
             << " CloseCode is : " << websocket_->closeCode() << " " << websocket_->errorString()
             << " must reconnect websocket after timeout";
  }
  /*setTimeout(setupWebSocket, 1000);*/
}

// ------------------------------------------------------------------
void qwebsocket_client::onSslErrors(const QList<QSslError>& errors)
{
  qDebug() << "Client::WebSocket SslErrors";
  Q_UNUSED(errors);

  // WARNING: Never ignore SSL errors in production code.
  // The proper way to handle self-signed certificates is to add a custom root
  // to the CA store.

  websocket_->ignoreSslErrors();
}

// ------------------------------------------------------------------
void qwebsocket_client::onError(QAbstractSocket::SocketError socketError)
{
  qDebug() << "Client::WebSocket Error";
  qDebug() << websocket_->errorString() << " Error:" << socketError;
}

// ------------------------------------------------------------------
void qwebsocket_client::onTextFrameReceived(const QString& frame, bool isLastFrame)
{
  // qDebug() << "TextFrameReceived : ";
}

// ------------------------------------------------------------------
void qwebsocket_client::onTextMessageReceived(QString message)
{
  qDebug() << "Client::WebSocket TextMessage received:" << message;
  emit processIncomingMessage(message);
}

// ------------------------------------------------------------------
void qwebsocket_client::onBinaryFrameReceived(const QByteArray& frame, bool isLastFrame)
{
  qDebug() << "Client::WebSocket BinaryFrameReceived : ";
}

// ------------------------------------------------------------------
void qwebsocket_client::onBinaryMessageReceived(const QByteArray& message)
{
  qDebug() << "Client::WebSocket BinaryMessageReceived : ";
}

// ------------------------------------------------------------------
void qwebsocket_client::onReadChannelFinished()
{
  qDebug() << "Client::WebSocket ReadChannelFinished : ";
}

// ------------------------------------------------------------------
void qwebsocket_client::onPong(quint64 elapsedTime, const QByteArray& payload)
{
  qDebug() << "Client::WebSocket Pong : ";
}

// ------------------------------------------------------------------
void qwebsocket_client::onBytesWritten(qint64 bytes)
{
  qDebug() << "Client::WebSocket BytesWritten : " << bytes;
}
