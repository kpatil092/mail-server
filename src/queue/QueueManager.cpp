#include "mailforge/queue/QueueManager.hpp"
#include <chrono>

#include "mailforge/smtp/SmtpClient.hpp"
#include <future>

namespace mailforge::queue {

QueueManager::QueueManager(database::DatabaseManager& db, boost::asio::io_context& io_context, logging::Logger& logger, std::size_t worker_threads)
    : db_(db), io_context_(io_context), logger_(logger), worker_count_(worker_threads) {}

QueueManager::~QueueManager() {
    stop();
}

void QueueManager::start() {
    if (running_) return;
    running_ = true;
    
    for (std::size_t i = 0; i < worker_count_; ++i) {
        workers_.emplace_back(&QueueManager::worker_loop, this);
    }
    logger_.info("QueueManager started with " + std::to_string(worker_count_) + " workers");
}

void QueueManager::stop() {
    if (!running_) return;
    running_ = false;
    cv_.notify_all();
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
    logger_.info("QueueManager stopped");
}

void QueueManager::notify() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        notified_ = true;
    }
    cv_.notify_one();
}

void QueueManager::worker_loop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::seconds(5), [this] { return !running_ || notified_; });
        
        notified_ = false;
        
        if (!running_) break;
        
        lock.unlock();
        while (process_one_message() && running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

bool QueueManager::process_one_message() {
    auto pending = db_.get_pending_messages();
    if (pending.empty()) {
        return false;
    }
    
    const auto& msg = pending.front();
    logger_.info("QueueManager processing message ID " + std::to_string(msg.id) + " from " + msg.sender + " to " + msg.recipient);
    
    std::promise<bool> delivery_promise;
    std::future<bool> delivery_future = delivery_promise.get_future();
    
    // Create SmtpClient and deliver the message
    smtp::SmtpClient client(io_context_, logger_);
    client.deliver(msg.sender, msg.recipient, msg.subject, msg.body,
                   [&delivery_promise](bool success) {
                       delivery_promise.set_value(success);
                   });
    
    // Wait for the delivery to complete
    bool success = false;
    try {
        success = delivery_future.get();
    } catch (const std::exception& e) {
        logger_.error("Exception during SMTP delivery of message ID " + std::to_string(msg.id) + ": " + e.what());
    }
    
    int next_attempts = msg.attempts + 1;
    
    if (success) {
        db_.update_message_status(msg.id, "delivered", next_attempts);
        logger_.info("QueueManager delivered message ID " + std::to_string(msg.id) + " successfully");
    } else {
        if (next_attempts >= 3) {
            db_.update_message_status(msg.id, "failed", next_attempts);
            logger_.error("QueueManager failed to deliver message ID " + std::to_string(msg.id) + " after maximum attempts");
        } else {
            db_.update_message_status(msg.id, "pending", next_attempts);
            logger_.warning("QueueManager failed to deliver message ID " + std::to_string(msg.id) + ", will retry (attempt " + std::to_string(next_attempts) + ")");
        }
    }
    
    return true;
}

} // namespace mailforge::queue
