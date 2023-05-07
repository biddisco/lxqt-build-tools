// belle client https example

#include "belle.hh"
namespace Belle = OB::Belle;

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

void on_http_error(Belle::Client& app)
{
  // set the http on error callback
  app.on_http_error([](auto& ctx) { std::cerr << "Error: " << ctx.ec.message() << "\n\n"; });
}

void websocket_subscribe_offers()
{
  std::string req_string =
    "{ \"id\": \"Example subscribe to XRP/GateHub USD order book\", \"command\": \"subscribe\", "
    "\"books\": [ { \"taker_pays\": { \"currency\": \"XRP\" }, \"taker_gets\": { \"currency\": "
    "\"USD\", \"issuer\": \"rhub8VRN55s94qWKDv6jmDy1pUykJzF3wq\" }, \"snapshot\": true } ] }";

  // init client with remote address, port, and ssl enabled
  Belle::Client app{"s1.ripple.com", 51234, true};
  on_http_error(app);

  // init an http request object
  Belle::Request req;

  nlohmann::json command;
  command["command"] = "subscribe";

  // buying xrp
  nlohmann::json buy_xrp;
  buy_xrp["taker_gets"]["currency"] = "XRP";
  buy_xrp["taker_pays"]["currency"] = "USD";
  buy_xrp["taker_pays"]["issuer"] = "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B";
  buy_xrp["snapshot"] = "true";
  // selling xrp
  nlohmann::json sell_xrp;
  sell_xrp["taker_pays"]["currency"] = "XRP";
  sell_xrp["taker_gets"]["currency"] = "USD";
  sell_xrp["taker_gets"]["issuer"] = "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B";
  sell_xrp["snapshot"] = "true";
  // subscribe to 2 books
  nlohmann::json subscription;
  subscription["books"] = nlohmann::json::array({buy_xrp, sell_xrp});

  command["books"] = nlohmann::json::array({buy_xrp, sell_xrp});

  nlohmann::json content;
  content["params"] = command;

  std::cout << "JSON text is : " << command << std::endl;

  // set the method
  req.method(Belle::Method::post);
  req.set(Belle::Header::host, "s1.ripple.com");
  req.set(Belle::Header::user_agent, "mystery");
  req.set(Belle::Header::content_type, "application/json");
  req.set(Belle::Header::accept, "application/json");
  //    req.set(Belle::Header::connection, "close");

  // set the target path
  req.target("/");
  req.body() = content.dump();
  req.prepare_payload();

  std::cout << req << std::endl;

  app.on_http(req.move(), [](auto& ctx) {
    // check http status code
    if (ctx.res.result() != Belle::Status::ok)
    {
      // print the response status code and reason
      std::cerr << "Error: " << ctx.res.result_int() << " " << ctx.res.reason() << "\n\n";
      return;
    }

    // print the full response
    std::cerr << ctx.res << "\n\n";
  });

  // start the client and save the number of completed requests
  auto completed = app.connect();

  const int sec = 25;
  // wait 5 seconds and collect some data
  for (int i = 0; i < sec; i++)
  {
    std::cout << "Closing in " << sec - i << " seconds " << std::endl;
    std::chrono::seconds dura(1);
    std::this_thread::sleep_for(dura);
  }

  std::cout << "Completed " << completed << std::endl;
}

int main(int argc, char* argv[])
{
  websocket_subscribe_offers();
  return 0;
}
