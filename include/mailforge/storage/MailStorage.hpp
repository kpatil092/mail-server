#pragma once

#include "mailforge/mime/MailMessage.hpp"

#include <filesystem>

namespace mailforge::storage {

class MailStorage {
public:
    explicit MailStorage(std::filesystem::path root);

    void store(const mime::MailMessage& message) const;

private:
    std::filesystem::path root_;
};

} // namespace mailforge::storage
