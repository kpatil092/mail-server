#include "mailforge/app/Application.hpp"

#include "mailforge/logging/Logger.hpp"
#include "mailforge/network/TcpListener.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <algorithm>
#include <csignal>
#include <sstream>
#include <thread>
#include <vector>

namespace mailforge::app {

Application::Application(config::ServerConfig config)
    : config_(std::move(config)) {}

int Application::run() {
    logging::Logger logger(logging::Logger::parse_level(config_.log_level));

    std::ostringstream startup;
    startup << "MailForge starting on " << config_.host << ':' << config_.port
            << " with " << config_.thread_pool << " worker threads";
    logger.info(startup.str());

    boost::asio::io_context io_context;
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) {
        logger.info("Shutdown signal received");
        io_context.stop();
    });

    network::TcpListener listener(io_context, config_, logger);
    listener.start();

    const auto worker_count = std::max<std::size_t>(1, config_.thread_pool);
    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    for (std::size_t i = 0; i < worker_count; ++i) {
        workers.emplace_back([&io_context] {
            io_context.run();
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    return 0;
}

} // namespace mailforge::app
