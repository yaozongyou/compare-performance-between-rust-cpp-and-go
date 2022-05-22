#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
 public:
  // Take ownership of the stream
  Session(tcp::socket&& socket, net::io_context* ioc);

  // Start the asynchronous operation
  void Run();

 private:
  void DoRead();
  void OnRead(beast::error_code ec, std::size_t bytes_transferred);
  void DoWrite();
  void OnWrite(beast::error_code ec, std::size_t bytes_transferred);
  void DoClose();

  net::io_context* ioc_;
  beast::tcp_stream stream_;
  beast::flat_buffer buffer_;
  http::request<http::string_body> req_;
  http::response<http::string_body> res_;
};
