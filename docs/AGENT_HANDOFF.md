# Agent Handoff

## Project

MailForge: C++20 portfolio mail server. Goal is a production-inspired SMTP/POP3-style backend, not a full enterprise MTA.

## Current State

- Phase 1 foundation exists: CMake, config, logger, SMTP parser/state machine, smoke tests.
- Phase 2 networking code has been added: Boost.Asio `TcpListener` and `ClientSession`.
- Phase 3 storage slice is done: `DATA` body is captured and stored as `.eml` files with normalized mailbox names and metadata headers.
- `mail/`, `logs/`, `build/`, `.venv/`, DB/log/key files are ignored via `.codexignore` / `.gitignore`.

## Important Files

- `CMakeLists.txt`: builds `mailforge_core`, `mailforge`, `mailforge_smoke_tests`; now requires Boost + Threads.
- `src/main.cpp`: loads config and runs app.
- `src/app/Application.cpp`: starts Boost.Asio server and worker threads.
- `src/network/TcpListener.cpp`: accepts TCP clients.
- `src/network/ClientSession.cpp`: SMTP socket session.
- `src/storage/MailStorage.cpp`: writes messages to filesystem mailboxes.
- `src/mime/MailMessage.cpp`: basic message model.
- `src/smtp/SMTPParser.cpp`: parses SMTP command lines.
- `src/smtp/SMTPStateMachine.cpp`: SMTP command order/state.
- `tests/smoke_tests.cpp`: config/parser/state-machine smoke test.
- `config/mailforge.json`: default host/port/thread/log/mail settings.

## Build Notes

Boost may be missing. User may need:

```bash
sudo apt-get update
sudo apt-get install -y libboost-dev
```

Then:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/mailforge config/mailforge.json
```

Manual SMTP check:

```bash
nc localhost 2525
```

## Current Next Work

Continue Phase 3 hardening.

Implement:

- scripted SMTP integration test
- better failure reply if storage write fails

Keep explanations short; user prefers low token usage.
