import socket
import sys

HOST = "127.0.0.1"
PORT = 2525

def get_response(sock):
    """Reads a single-line response from the server."""
    data = sock.recv(1024)
    response = data.decode("utf-8").strip()
    print(f"S: {response}")
    return response

def send_command(sock, command):
    """Sends a command to the server."""
    print(f"C: {command}")
    sock.sendall((command + "\r\n").encode("utf-8"))
    return get_response(sock)

def assert_starts_with(response, prefix):
    """Asserts that the server response starts with the expected code."""
    if not response.startswith(prefix):
        print(f"\n[FAIL] Expected response to start with '{prefix}', but got '{response}'")
        sys.exit(1)

def run_test():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((HOST, PORT))
        assert_starts_with(get_response(s), "220") # Initial greeting

        # --- Test Case 1: Successful delivery to a local recipient ---
        print("\n--- Running Test Case 1: Local Recipient (should succeed) ---")
        assert_starts_with(send_command(s, "EHLO test.client"), "250")
        assert_starts_with(send_command(s, "MAIL FROM:<test@example.com>"), "250")
        assert_starts_with(send_command(s, "RCPT TO:<user@mailforge.local>"), "250")
        assert_starts_with(send_command(s, "DATA"), "354")
        response = send_command(s, "Subject: Test 1\r\n\r\nHello.\r\n.")
        assert_starts_with(response, "250")
        print("--- Test Case 1 Passed ---")

        # --- Test Case 2: Rejected delivery to a non-local recipient ---
        print("\n--- Running Test Case 2: Non-Local Recipient (should fail) ---")
        assert_starts_with(send_command(s, "RSET"), "250")
        assert_starts_with(send_command(s, "MAIL FROM:<test@example.com>"), "250")
        assert_starts_with(send_command(s, "RCPT TO:<user@another-domain.com>"), "250")
        assert_starts_with(send_command(s, "DATA"), "354")
        response = send_command(s, "Subject: Test 2\r\n\r\nHello again.\r\n.")
        assert_starts_with(response, "554") # <-- Key assertion: Transaction failed
        print("--- Test Case 2 Passed ---")

        send_command(s, "QUIT")

if __name__ == "__main__":
    try:
        run_test()
        print("\n[SUCCESS] All client-side tests passed.")
    except ConnectionRefusedError:
        print("\n[FAIL] Connection refused. Is the mailforge server running?")
        sys.exit(1)
    except Exception as e:
        print(f"\n[FAIL] An unexpected error occurred: {e}")
        sys.exit(1)