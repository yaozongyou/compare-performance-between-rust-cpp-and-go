#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "session.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class NetworkThread;

// Accepts incoming connections and launches the sessions
class Acceptor : public std::enable_shared_from_this<Acceptor> {
 public:
  Acceptor(net::io_context& ioc, tcp::endpoint endpoint,
           std::shared_ptr<NetworkThread> network_thread);

  void Run();

 private:
  void DoAccept();
  void OnAccept(net::io_context* ioc, beast::error_code ec, tcp::socket socket);

  net::io_context& ioc_;
  tcp::acceptor acceptor_;
  std::shared_ptr<NetworkThread> network_thread_;
};
