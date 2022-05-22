#pragma once

#include <boost/asio.hpp>
#include <thread>

class NetworkThread {
 public:
  NetworkThread();
  ~NetworkThread();

  boost::asio::io_context& GetIoc();

  void Submit(boost::asio::ip::tcp::socket&& new_accepted_socket, boost::asio::io_context* ioc);

  void Stop();

 private:
  void ThreadMain(int thread_index);

  std::vector<std::thread> threads_;
  std::vector<boost::asio::io_context> iocs_;
  std::vector<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>*> guards_;

  int current_thread_index_;
};
