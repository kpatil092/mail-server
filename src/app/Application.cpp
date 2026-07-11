#include "mailforge/app/Application.hpp"
// Dummy comment to trigger clangd reparse of source
#include "mailforge/logging/Logger.hpp"
#include "mailforge/network/TcpListener.hpp"
#include "mailforge/database/DatabaseManager.hpp"
#include "mailforge/queue/QueueManager.hpp"
#include "mailforge/pop3/Pop3Server.hpp"

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

    database::DatabaseManager db(config_.database);
    // Register a default user for testing
    if (db.register_user("admin", "adminpass")) {
        logger.info("Registered default user: admin");
    }

    boost::asio::io_context io_context;
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) {
        logger.info("Shutdown signal received");
        io_context.stop();
    });

    queue::QueueManager queue_manager(db, io_context, logger);
    queue_manager.start();

    network::TcpListener listener(io_context, config_, logger, &db, &queue_manager);
    listener.start();

    pop3::Pop3Server pop3_server(io_context, config_, logger, db);
    pop3_server.start();

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
