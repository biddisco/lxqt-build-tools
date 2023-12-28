#pragma once

//
#include <QString>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtNetwork/QAbstractSocket>
#include <QtNetwork/QSslError>

class QWebSocket;

namespace net::ws {

  using rx_msg_handler_type = std::function<void(const QString message)>;

  class qwebsocket_client : public QObject
  {
    Q_OBJECT
public:
private:
    QWebSocket* websocket_;
    rx_msg_handler_type rx_handler_;
    QUrl url_;
    QString subscribe_;

public:
    explicit qwebsocket_client(const QUrl& url, const QString subscribe,
      const rx_msg_handler_type handler, QObject* parent = nullptr);
    ~qwebsocket_client();

    void startConnection();
    void stopConnection();

private Q_SLOTS:
    //
    void onConnected();
    void onDisconnected();
    void onStateChanged(QAbstractSocket::SocketState socketState);
    void onAboutToClose();
    //
    void onSslErrors(const QList<QSslError>& errors);
    void onError(QAbstractSocket::SocketError error);
    //
    void onTextFrameReceived(const QString& frame, bool isLastFrame);
    void onTextMessageReceived(QString message);
    //
    void onBinaryFrameReceived(const QByteArray& frame, bool isLastFrame);
    void onBinaryMessageReceived(const QByteArray& message);
    //
    void onReadChannelFinished();
    void onPong(quint64 elapsedTime, const QByteArray& payload);
    void onBytesWritten(qint64 bytes);

Q_SIGNALS:
    void processIncomingMessage(QString message);
    void finished();
  };
}    // namespace net::ws
