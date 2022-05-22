#include <boost/asio/signal_set.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "gflags/gflags.h"
#include "glog/logging.h"

#include "acceptor.h"
#include "network_thread.h"
#include "session.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

DEFINE_int32(bind_port, 3000, "bind port");

int main(int argc, char* argv[]) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  LOG(INFO) << "start greeting server";

  net::io_context ioc;

  net::signal_set signals(ioc, SIGINT, SIGTERM);
  signals.async_wait([&](beast::error_code const&, int) {
    LOG(INFO) << "got signal so try to stop ioc";
    ioc.stop();  // this will stop new accept
  });

  auto network_thread = std::make_shared<NetworkThread>();

  pthread_setname_np(pthread_self(), "acceptor");

  // Create and launch a listening port
  std::make_shared<Acceptor>(ioc,
                             net::ip::tcp::endpoint{net::ip::make_address("0.0.0.0"),
                                                    static_cast<uint16_t>(FLAGS_bind_port)},
                             network_thread)
      ->Run();

  ioc.run();

  network_thread->Stop();

  LOG(INFO) << "gracefully exit";

  return 0;
}