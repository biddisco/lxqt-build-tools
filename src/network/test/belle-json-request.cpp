#include <atomic>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>
#include "belle.hh"
namespace Belle = OB::Belle;

static std::atomic<int> pass_count{0};

// prototypes
void on_http_error(Belle::Client& app);

void on_http_error(Belle::Client& app)
{
  // set the http on error callback
  app.on_http_error([](auto& ctx) { std::cerr << "Error: " << ctx.ec.message() << "\n\n"; });
}

void xrpl_order_book()
{
  // curl command to query : buy xrp for USD.bitstamp
  // curl -H 'Content-Type: application/json' -d '{"method":"book_offers","params":[{"taker_gets":{"currency":"XRP"},"taker_pays":{"currency":"USD","issuer":"rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B"},"limit":10}]}' https://s1.ripple.com:51234/

  // init client with remote address, port, and ssl enabled
  Belle::Client app{"s1.ripple.com", 51234, true};
  on_http_error(app);

  // init an http request object
  Belle::Request req;

  nlohmann::json content;
  content["method"] = "book_offers";

  nlohmann::json paramlist;
  paramlist["taker_gets"]["currency"] = "XRP";
  paramlist["taker_pays"]["currency"] = "USD";
  paramlist["taker_pays"]["issuer"] = "rvYAfWj5gh67oV6fW32ZzP3Aw4Eubs59B";
  paramlist["limit"] = 10;

  content["params"] = nlohmann::json::array({paramlist});

  // set the method
  req.method(Belle::Method::post);
  req.set(Belle::Header::host, "s1.ripple.com");
  req.set(Belle::Header::user_agent, "mystery");
  req.set(Belle::Header::content_type, "application/json");
  req.set(Belle::Header::accept, "application/json");
  req.set(Belle::Header::connection, "close");
  // set the target path
  req.target("/");
  req.body() = content.dump();
  req.prepare_payload();

  app.on_http(req.move(), [](auto& ctx) {
    // check http status code
    if (ctx.res.result() != Belle::Status::ok)
    {
      // print the response status code and reason
      std::cerr << "Error: " << ctx.res.result_int() << " " << ctx.res.reason() << "\n\n";
      pass_count--;
      return;
    }

    // print the full response
    std::cerr << ctx.res << "\n\n";
    std::string result = ctx.res.body();
    if (result.find("{\"result\":{\"ledger_hash\":") != std::string::npos)
    {
      std::cout << "json quick check ok\n\n";
      pass_count++;
    }
    else
    {
      pass_count--;
    }
  });

  // save the number of requests in the queue
  auto total = app.queue().size();

  // start the client and save the number of completed requests
  auto completed = app.connect();
  std::cout << "Completed " << completed << " " << total << std::endl;
}

int main(int argc, char* argv[])
{
  xrpl_order_book();
  return pass_count.load() == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
