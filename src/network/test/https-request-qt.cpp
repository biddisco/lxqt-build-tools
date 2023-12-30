#include <iostream>
//
#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
//
#include <nlohmann/json.hpp>

static std::atomic<int> pass_count{0};

class qhttp_client : public QObject
{
  public:
  void startrequest()
  {
    const QUrl url("https://s1.ripple.com:51234");
    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    // request.setRawHeader("host", "s1.ripple.com");
    request.setRawHeader("user_agent", "mystery");
    request.setRawHeader("content_type", "application/json");
    request.setRawHeader("accept", "application/json");
    request.setRawHeader("connection", "close");

    nlohmann::json content;
    content["method"] = "book_offers";

    nlohmann::json paramlist;
    paramlist["taker_gets"]["currency"] = "XRP";
    paramlist["taker_pays"]["currency"] = "USD";
    paramlist["taker_pays"]["issuer"] = "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B";
    paramlist["limit"] = 10;

    content["params"] = nlohmann::json::array({paramlist});

    // set the target path
    // req.target("/");
    // req.body() = content.dump();
    // req.prepare_payload();

    // issue post request
    QNetworkReply* reply = networkmanager_.post(request, QByteArray(content.dump().data()));
    connect(
      &networkmanager_, &QNetworkAccessManager::finished, this, &qhttp_client::processrequest);
  }

  private:
  void processrequest(QNetworkReply* reply)
  {
    if (!reply)
      return;

    // print the full response
    QByteArray rep = reply->readAll();
    std::string result = QString(rep).toStdString();
    std::cerr << result << "\n\n";
    if (result.find("{\"result\":{\"ledger_hash\":") != std::string::npos)
    {
      std::cout << "json quick check ok\n\n";
      pass_count++;
      if (pass_count == 1)
      {
        QCoreApplication::quit();
      }
    }
    else
    {
      pass_count--;
    }
    //
    reply->deleteLater();
  }
  QNetworkAccessManager networkmanager_;
};

int main(int argc, char* argv[])
{
  QCoreApplication a(argc, argv);

  qhttp_client qhttp_client;
  qhttp_client.startrequest();
  a.exec();
  std::cout << "received " << pass_count.load() << std::endl;
  return pass_count.load() == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
