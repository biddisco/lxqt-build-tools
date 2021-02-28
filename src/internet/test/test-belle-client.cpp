// belle client https example

#include "belle.hh"
namespace Belle = OB::Belle;

#include <string>
#include <iostream>

void on_http_error(Belle::Client& app)
{
  // set the http on error callback
  app.on_http_error([](auto& ctx)
  {
    std::cerr << "Error: " << ctx.ec.message() << "\n\n";
  });
}


void request(Belle::Client &app, int index)
{
    std::cout << "Submitting request " << std::to_string(index) << std::endl;

    // http get request to path '/'
    app.on_http("/api/v2/ohlc/xrpusd/?step=60&limit=5", [index](auto& ctx)
    {
      // check http status code
      if (ctx.res.result() != Belle::Status::ok)
      {
        // print the response status code and reason
        std::cerr << "Error: " << ctx.res.result_int()
                  << " " << ctx.res.reason() << "\n\n";
        return;
      }

      // print the response headers and body
      std::cerr << "Response request " << std::to_string(index) << "\n" << ctx.res.body() << "\n\n";
    });
}

void http_post_lots()
{
  // init client with remote address, port, and ssl enabled
  Belle::Client app {"www.bitstamp.net", 443, true};
  on_http_error(app);

  for (int i=0; i<5; ++i) {
      request(app, i);
  }

  // save the number of requests in the queue
  auto total = app.queue().size();

  // start the client and save the number of completed requests
  auto completed = app.connect();

  // print the number of completed requests
  std::cerr << "connect: " << completed << "/" << total << "\n\n";

  for (int i=0; i<5; ++i) {
      request(app, i);
  }

  // save the number of requests in the queue
  total = app.queue().size();

  // start the client and save the number of completed requests
  completed = app.connect();

  std::cout << "Completed" << std::endl;
}

int main(int argc, char *argv[])
{
  // perform multiple http get requests to a single remote endpoint
  http_post_lots();
  return 0;
}
