#include "mailforge/database/DatabaseManager.hpp"

#include <openssl/sha.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace mailforge::database {

DatabaseManager::DatabaseManager(const std::filesystem::path& db_path) {
    if (sqlite3_open(db_path.string().c_str(), &db_) != SQLITE_OK) {
        std::string err_msg = "Failed to open database: ";
        if (db_) {
            err_msg += sqlite3_errmsg(db_);
            sqlite3_close(db_);
            db_ = nullptr;
        }
        throw std::runtime_error(err_msg);
    }
    initialize_schema();
}

DatabaseManager::~DatabaseManager() {
    if (db_) {
        sqlite3_close(db_);
    }
}

DatabaseManager::DatabaseManager(DatabaseManager&& other) noexcept
    : db_(other.db_) {
    other.db_ = nullptr;
}

DatabaseManager& DatabaseManager::operator=(DatabaseManager&& other) noexcept {
    if (this != &other) {
        if (db_) {
            sqlite3_close(db_);
        }
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

void DatabaseManager::initialize_schema() {
    const char* schema_sql = 
        "CREATE TABLE IF NOT EXISTS users ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username TEXT UNIQUE NOT NULL,"
        "  password_hash TEXT NOT NULL,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE TABLE IF NOT EXISTS messages ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  sender TEXT NOT NULL,"
        "  recipient TEXT NOT NULL,"
        "  subject TEXT,"
        "  body TEXT,"
        "  timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  status TEXT,"
        "  attempts INTEGER DEFAULT 0"
        ");"
        "CREATE TABLE IF NOT EXISTS attachments ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  message_id INTEGER REFERENCES messages(id),"
        "  filename TEXT NOT NULL,"
        "  size INTEGER NOT NULL,"
        "  path TEXT NOT NULL"
        ");";

    char* err_msg = nullptr;
    if (sqlite3_exec(db_, schema_sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
        std::string err(err_msg ? err_msg : "Unknown SQLite error");
        if (err_msg) {
            sqlite3_free(err_msg);
        }
        throw std::runtime_error("Failed to initialize schema: " + err);
    }
}

std::string DatabaseManager::hash_password(const std::string& password) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(password.c_str()), password.size(), hash);
    
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return oss.str();
}

bool DatabaseManager::register_user(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "INSERT INTO users (username, password_hash) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    std::string hashed = hash_password(password);
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hashed.c_str(), -1, SQLITE_TRANSIENT);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool DatabaseManager::verify_user(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "SELECT password_hash FROM users WHERE username = ?;";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    
    bool verified = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* hash_ptr = sqlite3_column_text(stmt, 0);
        if (hash_ptr) {
            std::string stored_hash(reinterpret_cast<const char*>(hash_ptr));
            verified = (stored_hash == hash_password(password));
        }
    }
    
    sqlite3_finalize(stmt);
    return verified;
}

std::int64_t DatabaseManager::store_message(const std::string& sender, 
                                           const std::string& recipient, 
                                           const std::string& subject, 
                                           const std::string& body, 
                                           const std::string& status) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "INSERT INTO messages (sender, recipient, subject, body, status) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare store_message statement");
    }
    
    sqlite3_bind_text(stmt, 1, sender.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, recipient.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, subject.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, body.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, status.c_str(), -1, SQLITE_TRANSIENT);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to insert message into database");
    }
    
    return sqlite3_last_insert_rowid(db_);
}

bool DatabaseManager::store_attachment(std::int64_t message_id, 
                                     const std::string& filename, 
                                     std::size_t size, 
                                     const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "INSERT INTO attachments (message_id, filename, size, path) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_int64(stmt, 1, message_id);
    sqlite3_bind_text(stmt, 2, filename.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(size));
    sqlite3_bind_text(stmt, 4, path.c_str(), -1, SQLITE_TRANSIENT);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

std::vector<DatabaseManager::PendingMessage> DatabaseManager::get_pending_messages() {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "SELECT id, sender, recipient, subject, body, attempts FROM messages WHERE status = 'pending';";
    sqlite3_stmt* stmt = nullptr;
    
    std::vector<PendingMessage> messages;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return messages;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PendingMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        
        const unsigned char* sender_ptr = sqlite3_column_text(stmt, 1);
        if (sender_ptr) msg.sender = reinterpret_cast<const char*>(sender_ptr);
        
        const unsigned char* recipient_ptr = sqlite3_column_text(stmt, 2);
        if (recipient_ptr) msg.recipient = reinterpret_cast<const char*>(recipient_ptr);
        
        const unsigned char* subject_ptr = sqlite3_column_text(stmt, 3);
        if (subject_ptr) msg.subject = reinterpret_cast<const char*>(subject_ptr);
        
        const unsigned char* body_ptr = sqlite3_column_text(stmt, 4);
        if (body_ptr) msg.body = reinterpret_cast<const char*>(body_ptr);
        
        msg.attempts = sqlite3_column_int(stmt, 5);
        messages.push_back(msg);
    }
    
    sqlite3_finalize(stmt);
    return messages;
}

void DatabaseManager::update_message_status(std::int64_t id, const std::string& status, int attempts) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "UPDATE messages SET status = ?, attempts = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare update_message_status statement");
    }
    
    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, attempts);
    sqlite3_bind_int64(stmt, 3, id);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to update message status in database");
    }
}

std::vector<DatabaseManager::PendingMessage> DatabaseManager::get_user_messages(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "SELECT id, sender, recipient, subject, body, attempts FROM messages WHERE (recipient = ? OR recipient = '<' || ? || '>') AND status != 'deleted';";
    sqlite3_stmt* stmt = nullptr;
    
    std::vector<PendingMessage> messages;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return messages;
    }
    
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        PendingMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        
        const unsigned char* sender_ptr = sqlite3_column_text(stmt, 1);
        if (sender_ptr) msg.sender = reinterpret_cast<const char*>(sender_ptr);
        
        const unsigned char* recipient_ptr = sqlite3_column_text(stmt, 2);
        if (recipient_ptr) msg.recipient = reinterpret_cast<const char*>(recipient_ptr);
        
        const unsigned char* subject_ptr = sqlite3_column_text(stmt, 3);
        if (subject_ptr) msg.subject = reinterpret_cast<const char*>(subject_ptr);
        
        const unsigned char* body_ptr = sqlite3_column_text(stmt, 4);
        if (body_ptr) msg.body = reinterpret_cast<const char*>(body_ptr);
        
        msg.attempts = sqlite3_column_int(stmt, 5);
        messages.push_back(msg);
    }
    
    sqlite3_finalize(stmt);
    return messages;
}

void DatabaseManager::delete_message(std::int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "UPDATE messages SET status = 'deleted' WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare delete_message statement");
    }
    
    sqlite3_bind_int64(stmt, 1, id);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to delete message in database");
    }
}

} // namespace mailforge::database
