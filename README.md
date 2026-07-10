# MailForge

MailForge is a production-inspired mail server project in modern C++20. The current milestone establishes a TCP SMTP listener, configuration, logging, SMTP command parsing, and an SMTP session state machine.

## Build

Install build dependencies:

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build g++ libboost-dev
```

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Run

```bash
./build/mailforge config/mailforge.json
```

Then connect from another terminal:

```bash
nc localhost 2525
```

## Current Scope

- Project layout from the MailForge overview
- Boost.Asio TCP listener and SMTP client sessions
- Flat JSON configuration loader for early settings
- Thread-safe console logger
- SMTP parser and state machine for `HELO`, `EHLO`, `MAIL FROM`, `RCPT TO`, `DATA`, `QUIT`, `NOOP`, and `RSET`
- Filesystem `.eml` storage under `mail/<recipient>/inbox/`
- Graceful shutdown on `SIGINT`/`SIGTERM`
- Smoke tests using plain `assert`

## Roadmap

1. Add filesystem mail storage for accepted messages.
2. Add authentication and SQLite persistence.
3. Add queue workers, retry policy, TLS, POP3, MIME, and benchmarks.
