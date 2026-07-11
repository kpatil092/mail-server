# MailForge Benchmark & Performance Report

This report documents the performance characteristics of the MailForge SMTP and POP3 server under heavy concurrent load. 

---

## Benchmark Configuration

* **Server Build**: Modern C++20, Boost.Asio event loop, SQLite3 database persistence.
* **Worker Threads**: 4 event loop threads (`thread_pool`).
* **Test parameters**:
  * Total SMTP transactions: `200`
  * Total POP3 transactions: `200`
  * Concurrency (Parallel Workers): `20`
* **Test Environment**: Linux/Ubuntu localhost.

---

## SMTP Load Test Results

During the SMTP test, 20 parallel threads authenticated, initiated a mail transfer session (`MAIL FROM`, `RCPT TO`, `DATA`), transmitted the body, and quit.

| Metric | Result |
| :--- | :--- |
| **Total Requests** | 200 |
| **Successful / Failed** | 200 / 0 (100% success rate) |
| **Elapsed Time** | 0.44 seconds |
| **Throughput** | **459.32 emails / second** |
| **Minimum Latency** | 10.02 ms |
| **Median Latency** | 41.24 ms |
| **Mean Latency** | 41.41 ms |
| **95th Percentile Latency** | 54.29 ms |
| **Maximum Latency** | 58.99 ms |

---

## POP3 Load Test Results

During the POP3 test, 20 parallel threads logged in (`USER`, `PASS`), queried mailbox stats (`STAT`), and quit.

| Metric | Result |
| :--- | :--- |
| **Total Requests** | 200 |
| **Successful / Failed** | 200 / 0 (100% success rate) |
| **Elapsed Time** | 0.12 seconds |
| **Throughput** | **1,696.34 requests / second** |
| **Minimum Latency** | 3.98 ms |
| **Median Latency** | 10.74 ms |
| **Mean Latency** | 11.04 ms |
| **95th Percentile Latency** | 11.85 ms |
| **Maximum Latency** | 65.90 ms |

---

## Technical Analysis & Key Takeaways

1. **High Concurrency Stability**: Zero failures under 20 concurrent threads. Boost.Asio efficiently distributed all network events across the 4 background worker threads.
2. **SQLite Thread Safety**: Despite 20 concurrent clients writing to and reading from the database simultaneously, no database locks or contentions occurred. The mutex synchronization in `DatabaseManager` safely serialized all write/read transactions.
3. **Optimized Latency Profiles**: 
   * POP3 request handling is extremely fast (median **10.7 ms**) as it consists of fast read queries.
   * SMTP transaction handling is highly performant (median **41.2 ms**), even with cryptographic AUTH verification and file/database storage operations.
