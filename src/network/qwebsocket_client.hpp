#pragma once

#include <atomic>
#include <string>
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

  using rx_msg_handler_type = std::function<void(QString const message)>;

  class qwebsocket_client : public QObject
  {
    Q_OBJECT
public:
private:
    std::atomic<QWebSocket*> websocket_;    // internal websocket
    rx_msg_handler_type rx_handler_;        // handler for message received
    QUrl url_;                              // the address/port
    QString subscribe_;                     // the channel subscription request
    std::string id_;                        // a name we use for debugging

public:
    explicit qwebsocket_client(std::string const& id, QUrl const& url, QString const subscribe,
        rx_msg_handler_type const handler, QObject* parent = nullptr);
    ~qwebsocket_client();

    std::string const& id() { return id_; }
    void startConnection();
    void stopConnection();

private Q_SLOTS:
    //
    void onConnected();
    void onDisconnected();
    void onStateChanged(QAbstractSocket::SocketState socketState);
    void onAboutToClose();
    //
    void onSslErrors(QList<QSslError> const& errors);
    void onError(QAbstractSocket::SocketError error);
    //
    void onTextFrameReceived(QString const& frame, bool isLastFrame);
    void onTextMessageReceived(QString message);
    //
    void onBinaryFrameReceived(QByteArray const& frame, bool isLastFrame);
    void onBinaryMessageReceived(QByteArray const& message);
    //
    void onReadChannelFinished();
    void onPong(quint64 elapsedTime, QByteArray const& payload);
    void onBytesWritten(qint64 bytes);

Q_SIGNALS:
    void processIncomingMessage(QString message);
    void finished();
  };
}    // namespace net::ws
