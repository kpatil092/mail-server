import socket
import time
from concurrent.futures import ThreadPoolExecutor
import statistics

# SMTP Server details
SMTP_HOST = "127.0.0.1"
SMTP_PORT = 2525

# POP3 Server details
POP_HOST = "127.0.0.1"
POP_PORT = 1100

NUM_EMAILS = 200
CONCURRENCY = 20

def send_one_email(index):
    start = time.perf_counter()
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((SMTP_HOST, SMTP_PORT))
            s.recv(1024) # Greeting
            
            s.sendall(b"EHLO client.local\r\n")
            s.recv(1024)
            
            # Authenticate as admin / adminpass
            s.sendall(b"AUTH PLAIN AGFkbWluAGFkbWlucGFzcw==\r\n")
            s.recv(1024)
            
            s.sendall(f"MAIL FROM:<sender_{index}@example.com>\r\n".encode())
            s.recv(1024)
            
            s.sendall(f"RCPT TO:<admin>\r\n".encode())
            s.recv(1024)
            
            s.sendall(b"DATA\r\n")
            s.recv(1024)
            
            s.sendall(f"Subject: Load Test {index}\r\n\r\nThis is mail body number {index}.\r\n.\r\n".encode())
            s.recv(1024)
            
            s.sendall(b"QUIT\r\n")
            s.recv(1024)
        return True, time.perf_counter() - start
    except Exception as e:
        return False, 0.0

def retrieve_one_email(index):
    start = time.perf_counter()
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((POP_HOST, POP_PORT))
            s.recv(1024) # Greeting
            
            s.sendall(b"USER admin\r\n")
            s.recv(1024)
            
            s.sendall(b"PASS adminpass\r\n")
            s.recv(1024)
            
            s.sendall(b"STAT\r\n")
            s.recv(1024)
            
            s.sendall(b"QUIT\r\n")
            s.recv(1024)
        return True, time.perf_counter() - start
    except Exception as e:
        return False, 0.0

def run_smtp_benchmark():
    print(f"Starting SMTP Load Test: {NUM_EMAILS} emails, Concurrency: {CONCURRENCY}...")
    start_time = time.perf_counter()
    
    latencies = []
    success_count = 0
    failure_count = 0
    
    with ThreadPoolExecutor(max_workers=CONCURRENCY) as executor:
        results = list(executor.map(send_one_email, range(NUM_EMAILS)))
        
    total_time = time.perf_counter() - start_time
    
    for success, latency in results:
        if success:
            success_count += 1
            latencies.append(latency)
        else:
            failure_count += 1
            
    print_results("SMTP", success_count, failure_count, latencies, total_time)

def run_pop_benchmark():
    print(f"Starting POP3 Load Test: {NUM_EMAILS} requests, Concurrency: {CONCURRENCY}...")
    start_time = time.perf_counter()
    
    latencies = []
    success_count = 0
    failure_count = 0
    
    with ThreadPoolExecutor(max_workers=CONCURRENCY) as executor:
        results = list(executor.map(retrieve_one_email, range(NUM_EMAILS)))
        
    total_time = time.perf_counter() - start_time
    
    for success, latency in results:
        if success:
            success_count += 1
            latencies.append(latency)
        else:
            failure_count += 1
            
    print_results("POP3", success_count, failure_count, latencies, total_time)

def print_results(protocol, success, failure, latencies, total_time):
    print(f"\n================ {protocol} Benchmark Results ================")
    print(f"Total Requests: {success + failure}")
    print(f"Successful: {success}")
    print(f"Failed: {failure}")
    print(f"Total Elapsed Time: {total_time:.2f} seconds")
    print(f"Throughput: {success / total_time:.2f} requests/sec")
    if latencies:
        print(f"Latency Min: {min(latencies)*1000:.2f} ms")
        print(f"Latency Median: {statistics.median(latencies)*1000:.2f} ms")
        print(f"Latency Mean: {statistics.mean(latencies)*1000:.2f} ms")
        print(f"Latency Max: {max(latencies)*1000:.2f} ms")
        if len(latencies) >= 20:
            print(f"Latency 95th Percentile: {statistics.quantiles(latencies, n=20)[18]*1000:.2f} ms")
    print("========================================================\n")

if __name__ == "__main__":
    run_smtp_benchmark()
    run_pop_benchmark()
