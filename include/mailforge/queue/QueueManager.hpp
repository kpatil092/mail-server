#pragma once

#include "mailforge/database/DatabaseManager.hpp"
#include "mailforge/logging/Logger.hpp"

#include <boost/asio/io_context.hpp>
#include <atomic>
#include <thread>
#include <vector>
#include <condition_variable>
#include <mutex>

namespace mailforge::queue {

class QueueManager {
public:
    QueueManager(database::DatabaseManager& db, boost::asio::io_context& io_context, logging::Logger& logger, std::size_t worker_threads = 2);
    ~QueueManager();

    // Disable copy/move
    QueueManager(const QueueManager&) = delete;
    QueueManager& operator=(const QueueManager&) = delete;

    void start();
    void stop();

    // Signal workers to check database for pending messages
    void notify();

private:
    void worker_loop();
    bool process_one_message();

    database::DatabaseManager& db_;
    boost::asio::io_context& io_context_;
    logging::Logger& logger_;
    std::size_t worker_count_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    bool notified_{false};
};

} // namespace mailforge::queue
