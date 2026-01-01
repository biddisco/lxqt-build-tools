#pragma once

#include <string>
//
#include <boost/program_options.hpp>
#include <nlohmann/json.hpp>

using options_map = std::map<std::string, std::string>;

static boost::program_options::variables_map set_program_options(
    int argc, char* argv[], options_map args = {})
{
  std::string url = args.contains("url") ? args["url"] : "s1.ripple.com";
  std::string port = args.contains("port") ? args["port"] : "443";
  std::string target = args.contains("target") ? args["target"] : "";
  std::string timeout = args.contains("timeout") ? args["timeout"] : "";
  //
  namespace po = boost::program_options;
  po::options_description desc("Options");
  // clang-format off
  desc.add_options()
      ("port,p", po::value<std::string>()->default_value(port),
          "port number for http(s) server")
      ("url,u", po::value<std::string>()->default_value(url),
          "url for http(s) server")
      ("target,t", po::value<std::string>()->default_value(target),
          "target string to pass to request")
      ("timeout", po::value<int>()->default_value(25),
          "timeout in seconds for request")
      ;
  // clang-format on

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).allow_unregistered().options(desc).run(), vm);
  po::notify(vm);

  if (vm.count("url")) std::cout << "Using URL: " << vm["url"].as<std::string>() << "\n";
  if (vm.count("port")) std::cout << "Using port: " << vm["port"].as<std::string>() << "\n";
  if (vm.count("target")) std::cout << "Using target: " << vm["target"].as<std::string>() << "\n";
  if (vm.count("timeout")) std::cout << "Using timeout: " << vm["timeout"].as<int>() << "\n";

  return vm;
}
