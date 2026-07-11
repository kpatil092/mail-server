#pragma once

#include <string>
#include <vector>

namespace mailforge::mime {

struct Attachment {
    std::string filename;
    std::string content_type;
    std::string data; // decoded data
};

class MimeParser {
public:
    explicit MimeParser(const std::string& raw_content);

    [[nodiscard]] const std::string& text_body() const;
    [[nodiscard]] const std::string& html_body() const;
    [[nodiscard]] const std::vector<Attachment>& attachments() const;

private:
    void parse();
    void parse_part(const std::string& part_content);
    std::string extract_boundary(const std::string& content_type_header);

    std::string raw_content_;
    std::string text_body_;
    std::string html_body_;
    std::vector<Attachment> attachments_;
};

} // namespace mailforge::mime
