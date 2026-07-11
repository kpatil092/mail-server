#include "mailforge/mime/MimeParser.hpp"
#include "mailforge/utils/Base64.hpp"
#include <sstream>
#include <regex>
#include <algorithm>
#include <iostream>

namespace mailforge::mime {

MimeParser::MimeParser(const std::string& raw_content)
    : raw_content_(raw_content) {
    parse();
}

const std::string& MimeParser::text_body() const { return text_body_; }
const std::string& MimeParser::html_body() const { return html_body_; }
const std::vector<Attachment>& MimeParser::attachments() const { return attachments_; }

std::string MimeParser::extract_boundary(const std::string& content_type_header) {
    std::regex boundary_regex("boundary=\"?([^\";]+)\"?");
    std::smatch match;
    if (std::regex_search(content_type_header, match, boundary_regex) && match.size() == 2) {
        return match[1].str();
    }
    return "";
}

void MimeParser::parse() {
    // Separate main headers and body
    std::size_t header_end = raw_content_.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        header_end = raw_content_.find("\n\n");
    }

    if (header_end == std::string::npos) {
        // No headers, treat entire content as plain text body
        text_body_ = raw_content_;
        return;
    }

    std::string headers_part = raw_content_.substr(0, header_end);
    std::string body_part = raw_content_.substr(header_end + (raw_content_.find("\r\n\r\n") != std::string::npos ? 4 : 2));

    // Parse Content-Type header to see if it's multipart
    std::string content_type;
    std::istringstream iss(headers_part);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.back() == '\r') line.pop_back();
        if (line.rfind("Content-Type:", 0) == 0) {
            content_type = line.substr(13);
            break;
        }
    }

    std::string boundary = extract_boundary(content_type);
    if (boundary.empty()) {
        // Single part email
        if (content_type.find("text/html") != std::string::npos) {
            html_body_ = body_part;
        } else {
            text_body_ = body_part;
        }
        return;
    }

    // Split body by boundary
    std::string delimiter = "--" + boundary;
    std::size_t pos = body_part.find(delimiter);
    while (pos != std::string::npos) {
        std::size_t next_pos = body_part.find(delimiter, pos + delimiter.size());
        if (next_pos == std::string::npos) {
            break; // End boundary or malformed
        }

        std::size_t part_start = pos + delimiter.size();
        // Trim leading CRLF/LF from part
        if (body_part.compare(part_start, 2, "\r\n") == 0) part_start += 2;
        else if (body_part.compare(part_start, 1, "\n") == 0) part_start += 1;

        std::size_t part_len = next_pos - part_start;
        // Trim trailing CRLF/LF
        if (part_len > 2 && body_part.compare(part_start + part_len - 2, 2, "\r\n") == 0) part_len -= 2;
        else if (part_len > 1 && body_part.compare(part_start + part_len - 1, 1, "\n") == 0) part_len -= 1;

        std::string part_content = body_part.substr(part_start, part_len);
        if (part_content != "--") { // Skip end boundary suffix
            parse_part(part_content);
        }

        pos = next_pos;
    }
}

void MimeParser::parse_part(const std::string& part_content) {
    std::size_t header_end = part_content.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        header_end = part_content.find("\n\n");
    }

    if (header_end == std::string::npos) {
        return;
    }

    std::string headers = part_content.substr(0, header_end);
    std::string body = part_content.substr(header_end + (part_content.find("\r\n\r\n") != std::string::npos ? 4 : 2));

    std::string content_type;
    std::string disposition;
    std::string encoding;

    std::istringstream iss(headers);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.back() == '\r') line.pop_back();
        if (line.rfind("Content-Type:", 0) == 0) {
            content_type = line.substr(13);
        } else if (line.rfind("Content-Disposition:", 0) == 0) {
            disposition = line.substr(20);
        } else if (line.rfind("Content-Transfer-Encoding:", 0) == 0) {
            encoding = line.substr(26);
        }
    }

    // Trim encoding string
    encoding.erase(std::remove_if(encoding.begin(), encoding.end(), ::isspace), encoding.end());
    std::transform(encoding.begin(), encoding.end(), encoding.begin(), ::tolower);

    // Check if it is an attachment
    std::regex filename_regex("filename=\"?([^\";]+)\"?");
    std::smatch match;
    if (std::regex_search(disposition, match, filename_regex) && match.size() == 2) {
        Attachment att;
        att.filename = match[1].str();
        
        // Extract type
        std::size_t semi = content_type.find(';');
        att.content_type = (semi == std::string::npos) ? content_type : content_type.substr(0, semi);
        att.content_type.erase(std::remove_if(att.content_type.begin(), att.content_type.end(), ::isspace), att.content_type.end());

        if (encoding == "base64") {
            // Strip any whitespace/newlines from base64 string
            body.erase(std::remove_if(body.begin(), body.end(), ::isspace), body.end());
            att.data = utils::base64_decode(body);
        } else {
            att.data = body;
        }

        attachments_.push_back(att);
        return;
    }

    // Otherwise, it's body text/html
    if (content_type.find("text/html") != std::string::npos) {
        html_body_ = body;
    } else if (content_type.find("text/plain") != std::string::npos) {
        text_body_ = body;
    }
}

} // namespace mailforge::mime
