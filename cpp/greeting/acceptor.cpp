#include "acceptor.h"

#include <glog/logging.h>

#include "network_thread.h"

Acceptor::Acceptor(net::io_context& ioc, tcp::endpoint endpoint,
                   std::shared_ptr<NetworkThread> network_thread)
    : ioc_(ioc), acceptor_(net::make_strand(ioc)), network_thread_(network_thread) {
  beast::error_code ec;

  // Open the acceptor
  acceptor_.open(endpoint.protocol(), ec);
  if (ec) {
    LOG(ERROR) << "failed to open: " << ec;
    return;
  }

  // Allow address reuse
  acceptor_.set_option(net::socket_base::reuse_address(true), ec);
  if (ec) {
    LOG(ERROR) << "failed to set_option: " << ec;
    return;
  }

  // Bind to the server address
  acceptor_.bind(endpoint, ec);
  if (ec) {
    LOG(ERROR) << "failed to bind: " << ec;
    return;
  }

  // Start listening for connections
  acceptor_.listen(net::socket_base::max_listen_connections, ec);
  if (ec) {
    LOG(ERROR) << "failed to listen: " << ec;
    return;
  }
}

void Acceptor::Run() { DoAccept(); }

void Acceptor::DoAccept() {
  net::io_service& ioc = network_thread_->GetIoc();
  acceptor_.async_accept(ioc,
                         beast::bind_front_handler(&Acceptor::OnAccept, shared_from_this(), &ioc));
}

void Acceptor::OnAccept(net::io_context* ioc, beast::error_code ec, tcp::socket socket) {
  LOG(INFO) << "OnAccept: ec " << ec.message() << " peer address "
            << socket.remote_endpoint().address().to_string() << ":"
            << socket.remote_endpoint().port();

  if (ec) {
    LOG(ERROR) << "failed to accept: " << ec;
  } else {
    // Create the session and run it
    // std::make_shared<SelectSession>(std::move(socket))->run();
    network_thread_->Submit(std::move(socket), ioc);
  }

  // Accept another connection
  DoAccept();
}
