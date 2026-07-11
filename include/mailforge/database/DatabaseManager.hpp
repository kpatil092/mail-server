#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <mutex>
#include <sqlite3.h>

namespace mailforge::database {

class DatabaseManager {
public:
    explicit DatabaseManager(const std::filesystem::path& db_path);
    ~DatabaseManager();

    // Disable copy
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    // Support move
    DatabaseManager(DatabaseManager&& other) noexcept;
    DatabaseManager& operator=(DatabaseManager&& other) noexcept;

    // User management
    bool register_user(const std::string& username, const std::string& password);
    bool verify_user(const std::string& username, const std::string& password);

    struct PendingMessage {
        std::int64_t id;
        std::string sender;
        std::string recipient;
        std::string subject;
        std::string body;
        int attempts;
    };

    // Message management
    std::int64_t store_message(const std::string& sender, 
                               const std::string& recipient, 
                               const std::string& subject, 
                               const std::string& body, 
                               const std::string& status);

    std::vector<PendingMessage> get_pending_messages();
    std::vector<PendingMessage> get_user_messages(const std::string& username);
    void update_message_status(std::int64_t id, const std::string& status, int attempts);
    void delete_message(std::int64_t id);

    bool store_attachment(std::int64_t message_id, 
                         const std::string& filename, 
                         std::size_t size, 
                         const std::string& path);

    // Hash helper
    static std::string hash_password(const std::string& password);

private:
    void initialize_schema();
    sqlite3* db_{nullptr};
    mutable std::mutex mutex_;
};

} // namespace mailforge::database
