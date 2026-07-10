# MailForge Overview Summary

MailForge is a C++20, Linux-first, production-inspired mail server for portfolio and interview use. It is not intended to replace enterprise MTAs, but to demonstrate networking, protocol design, concurrency, security, persistence, testing, and production-style engineering.

## Core Goals

- Accept mail over SMTP
- Store and retrieve email
- Manage users and authentication
- Maintain delivery queues
- Support encrypted communication
- Generate logs and audit trails
- Remain modular, testable, performant, and easy to extend

## Intended Stack

- C++20
- CMake
- GCC or Clang
- Boost.Asio
- OpenSSL
- SQLite
- spdlog
- nlohmann/json
- GoogleTest
- Docker

## Main Modules

- Network layer
- SMTP module
- Authentication module
- Mail storage module
- Queue module
- Database module
- Logging module
- Configuration module
- Utility, MIME, and security helpers

## Resume Version

The strong backend-focused version includes SMTP, POP3 or IMAP, authentication, queueing, SQLite, TLS, logging, Docker, and tests. A frontend is optional and should come after the backend is reliable.
