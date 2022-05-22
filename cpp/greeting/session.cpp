#include "session.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <string>

#include "absl/strings/str_split.h"
#include "glog/logging.h"

// copy from https://localcoder.org/encode-decode-urls-in-c
std::string url_decode(std::string str) {
  std::string ret;
  char ch;
  int i, ii, len = str.length();

  for (i = 0; i < len; i++) {
    if (str[i] != '%') {
      if (str[i] == '+')
        ret += ' ';
      else
        ret += str[i];
    } else {
      sscanf(str.substr(i + 1, 2).c_str(), "%x", &ii);
      ch = static_cast<char>(ii);
      ret += ch;
      i = i + 2;
    }
  }
  return ret;
}

std::string extract_name_from_query_string(std::string query_string) {
  std::vector<std::string> values = absl::StrSplit(query_string, "&");
  for (auto& str : values) {
    std::vector<std::string> kv = absl::StrSplit(str, "=");
    if (kv.size() < 2) {
      continue;
    }

    if (kv[0] == "name") {
      return kv[1];
    }
  }
  return "";
}

Session::Session(tcp::socket&& socket, net::io_context* ioc)
    : ioc_(ioc), stream_(std::move(socket)) {}

// Start the asynchronous operation
void Session::Run() {
  net::post(stream_.get_executor(),
            beast::bind_front_handler(&Session::DoRead, shared_from_this()));
}

void Session::DoRead() {
  // Make the request empty before reading, otherwise the operation behavior is undefined.
  req_ = {};
  http::async_read(stream_, buffer_, req_,
                   beast::bind_front_handler(&Session::OnRead, shared_from_this()));
}

void Session::OnRead(beast::error_code ec, std::size_t bytes_transferred) {
  if (ec == http::error::end_of_stream) {
    LOG(INFO) << "OnRead returned end_of_stream, just close";
    return DoClose();
  }

  if (ec) {
    LOG(ERROR) << "OnRead returned error " << ec.message() << ", just close";
    return DoClose();
  }

  if (req_.method() != http::verb::get) {
    res_ = {http::status::bad_request, req_.version()};
    DoWrite();
    return;
  }

  std::vector<std::string> v =
      absl::StrSplit(absl::string_view(req_.target().data(), req_.target().length()), "?");
  if (v.size() != 2) {
    res_ = {http::status::bad_request, req_.version()};
    DoWrite();
    return;
  }

  if (v[0] != "/greeting") {
    res_ = {http::status::bad_request, req_.version()};
    DoWrite();
    return;
  }

  std::string name = extract_name_from_query_string(v[1]);
  std::string greeting = "Hello " + url_decode(name);

  res_ = {http::status::ok, req_.version()};
  res_.set(http::field::content_type, "text/plain; charset=utf-8");
  res_.body() = greeting;
  res_.set(http::field::content_length, greeting.size());

  DoWrite();
}

void Session::DoWrite() {
  http::async_write(stream_, res_,
                    beast::bind_front_handler(&Session::OnWrite, shared_from_this()));
}

void Session::OnWrite(beast::error_code ec, std::size_t bytes_transferred) {
  if (ec) {
    LOG(ERROR) << "OnWrite error " << ec.message();
    return DoClose();
  }

  DoRead();
}

void Session::DoClose() {
  stream_.close();
  LOG(INFO) << "connection with downstream has been closed";
}
