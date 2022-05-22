#include "network_thread.h"

#include <glog/logging.h>

#include "session.h"

DEFINE_int32(network_thread_number, 8, "network thread number");

NetworkThread::NetworkThread()
    : iocs_(FLAGS_network_thread_number),
      guards_(FLAGS_network_thread_number),
      current_thread_index_(0) {
  threads_.reserve(FLAGS_network_thread_number);

  for (int i = 0; i < FLAGS_network_thread_number; i++) {
    threads_.emplace_back(&NetworkThread::ThreadMain, this, i);
  }
}

NetworkThread::~NetworkThread() {}

void NetworkThread::ThreadMain(int thread_index) {
  LOG(INFO) << "network thread started: thread_index " << thread_index;

  std::string thread_name = "network_" + std::to_string(thread_index);

  pthread_setname_np(pthread_self(), thread_name.c_str());

  // prevernt io_context from running out of work
  guards_[thread_index] =
      new boost::asio::executor_work_guard<boost::asio::io_context::executor_type>(
          iocs_[thread_index].get_executor());

  iocs_[thread_index].run();

  LOG(INFO) << "network thread stoped: thread_index " << thread_index;
}

void NetworkThread::Stop() {
  // reset tracked work, allow io_context.run() to exit
  for (auto& guard : guards_) {
    delete guard;
  }

  for (auto& ioc : iocs_) {
    ioc.stop();
  }

  for (auto& t : threads_) {
    t.join();
  }
}

void NetworkThread::Submit(boost::asio::ip::tcp::socket&& new_accepted_socket,
                           boost::asio::io_context* ioc) {
  std::make_shared<Session>(std::move(new_accepted_socket), ioc)->Run();
}

boost::asio::io_context& NetworkThread::GetIoc() {
  if (++current_thread_index_ >= static_cast<int>(threads_.size())) {
    current_thread_index_ = 0;
  }

  return iocs_[current_thread_index_];
}
