# MailForge Project Plan

## Phase 1: Foundation

- CMake repository setup
- Configuration module
- Logging module
- Initial SMTP parser and state machine
- Smoke tests
- Developer ignore rules for generated/heavy files

## Phase 2: Networking

- [x] Boost.Asio TCP listener
- [x] Client sessions
- [x] Async reads and writes
- [x] Thread pool ownership
- [x] Connection limits

## Phase 3: SMTP Receive Path

- Full command validation
- [x] `DATA` buffering and message finalization
- [x] Mailbox delivery into filesystem storage
- [x] Storage smoke test
- [x] Address normalization and message metadata headers
- Integration tests using scripted SMTP sessions

## Later Phases

- Authentication and SQLite
- Queue workers and outgoing SMTP delivery
- STARTTLS
- POP3 retrieval
- MIME parsing
- Benchmarking and Docker packaging
