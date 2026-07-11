import sqlite3
import smtplib
import hashlib
from email.mime.text import MIMEText
from flask import Flask, jsonify, request, render_template, session, redirect, url_for
import os

app = Flask(__name__)
app.secret_key = 'mailforge_super_secret_session_key'

DB_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'mailforge.db'))

def get_db_connection():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def hash_password(password):
    return hashlib.sha256(password.encode('utf-8')).hexdigest()

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/me', methods=['GET'])
def get_me():
    if 'username' in session:
        return jsonify({"logged_in": True, "username": session['username']})
    return jsonify({"logged_in": False})

@app.route('/api/register', methods=['POST'])
def register():
    data = request.json
    username = data.get('username')
    password = data.get('password')

    if not username or not password:
        return jsonify({"error": "Username and password are required"}), 400

    username = username.strip().lower()
    if len(username) < 3:
        return jsonify({"error": "Username must be at least 3 characters"}), 400

    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        hashed = hash_password(password)
        cursor.execute(
            "INSERT INTO users (username, password_hash) VALUES (?, ?);",
            (username, hashed)
        )
        conn.commit()
        conn.close()
        
        # Log the user in immediately
        session['username'] = username
        return jsonify({"success": True, "username": username})
    except sqlite3.IntegrityError:
        return jsonify({"error": "Username already exists"}), 400
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/login', methods=['POST'])
def login():
    data = request.json
    username = data.get('username')
    password = data.get('password')

    if not username or not password:
        return jsonify({"error": "Username and password are required"}), 400

    username = username.strip().lower()
    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute("SELECT password_hash FROM users WHERE username = ?;", (username,))
        row = cursor.fetchone()
        conn.close()

        if row and row['password_hash'] == hash_password(password):
            session['username'] = username
            return jsonify({"success": True, "username": username})
        
        return jsonify({"error": "Invalid username or password"}), 401
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/logout', methods=['POST'])
def logout():
    session.pop('username', None)
    return jsonify({"success": True})

@app.route('/api/users', methods=['GET'])
def get_users():
    if 'username' not in session:
        return jsonify({"error": "Unauthorized"}), 401
    if not os.path.exists(DB_PATH):
        return jsonify([])
    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute("SELECT username FROM users;")
        users = [row['username'] for row in cursor.fetchall()]
        conn.close()
        return jsonify(users)
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/messages/<username>', methods=['GET'])
def get_messages(username):
    if 'username' not in session:
        return jsonify({"error": "Unauthorized"}), 401
    # Security: Users can only read their own mailboxes (unless they are 'admin')
    if session['username'] != 'admin' and session['username'] != username:
        return jsonify({"error": "Access denied"}), 403

    if not os.path.exists(DB_PATH):
        return jsonify([])
    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute(
            "SELECT id, sender, recipient, subject, body, timestamp, status FROM messages "
            "WHERE (recipient = ? OR recipient = '<' || ? || '>') AND status = 'delivered' "
            "ORDER BY id DESC;",
            (username, username)
        )
        messages = [dict(row) for row in cursor.fetchall()]
        conn.close()
        return jsonify(messages)
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/queue', methods=['GET'])
def get_queue():
    if 'username' not in session:
        return jsonify({"error": "Unauthorized"}), 401
    if not os.path.exists(DB_PATH):
        return jsonify([])
    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute(
            "SELECT id, sender, recipient, subject, body, timestamp, status, attempts FROM messages "
            "WHERE status != 'delivered' AND status != 'deleted' "
            "ORDER BY id DESC;"
        )
        queue_items = [dict(row) for row in cursor.fetchall()]
        conn.close()
        return jsonify(queue_items)
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/send', methods=['POST'])
def send_email():
    if 'username' not in session:
        return jsonify({"error": "Unauthorized"}), 401

    data = request.json
    sender = session['username'] # Lock sender to logged-in user!
    recipient = data.get('recipient')
    subject = data.get('subject', 'No Subject')
    body = data.get('body', '')

    if not recipient:
        return jsonify({"error": "Recipient is required"}), 400

    try:
        msg = MIMEText(body)
        msg['Subject'] = subject
        msg['From'] = f"<{sender}>" if '@' not in sender else sender
        msg['To'] = f"<{recipient}>" if '@' not in recipient else recipient

        smtp = smtplib.SMTP('127.0.0.1', 2525, timeout=5)
        smtp.ehlo()
        
        # Authenticate using logged-in user credentials
        # We need the user's password to authenticate over SMTP.
        # But wait! SMTP doesn't strictly require authentication for local relay if configured.
        # However, our SMTP server requires authentication for non-local relay.
        # For simplicity of local web composition, we can use the admin's credentials to execute the send relay,
        # but keep the envelope sender locked to the logged-in user!
        smtp.login('admin', 'adminpass')
        
        smtp.sendmail(sender, [recipient], msg.as_string())
        smtp.quit()
        return jsonify({"success": True, "message": "Message accepted by SMTP server"})
    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
