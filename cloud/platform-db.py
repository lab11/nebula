import sqlite3
import secrets
from concurrent.futures import ThreadPoolExecutor

# Connect to the database (or create it if it does not exist)
conn = sqlite3.connect('keys.db')
cursor = conn.cursor()

# Create the 'keys' table if it does not exist
cursor.execute('''
CREATE TABLE IF NOT EXISTS keys (
    id INTEGER PRIMARY KEY,
    key TEXT UNIQUE,
    used INTEGER
)
''')
conn.commit()

def generate_key():
    # Generate a random 128-bit key (32 hex characters)
    return secrets.token_hex(16)

def insert_key(key, used):
    with sqlite3.connect('keys.db') as conn:
        cursor = conn.cursor()
        try:
            cursor.execute("INSERT INTO keys (key, used) VALUES (?, ?)", (key, used))
            conn.commit()
            return True
        except sqlite3.IntegrityError:
            return False

def get_key(used):
    with sqlite3.connect('keys.db') as conn:
        cursor = conn.cursor()
        cursor.execute("SELECT * FROM keys WHERE used=? ORDER BY RANDOM() LIMIT 1", (used,))
        row = cursor.fetchone()
        if row:
            return row[1]
        else:
            return None

def mark_key_as_used(key):
    with sqlite3.connect('keys.db') as conn:
        cursor = conn.cursor()
        cursor.execute("UPDATE keys SET used=1 WHERE key=?", (key,))
        conn.commit()

def insert_key_task(unused):
    while not insert_key(generate_key(), 0):
        pass

def insert_and_retrieve_keys(n):
    # Insert n unique keys
    with ThreadPoolExecutor(max_workers=32) as executor:
        executor.map(insert_key_task, range(n))

    # Retrieve and display unused keys
    unused_keys = []
    used_keys = []

    while (key := get_key(0)) is not None:
        unused_keys.append(key)
        mark_key_as_used(key)

    print(f"Unused keys: {unused_keys}")

    # Retrieve and display used keys
    with sqlite3.connect('keys.db') as conn:
        cursor = conn.cursor()
        cursor.execute("SELECT * FROM keys WHERE used=1")
        used_keys = [row[1] for row in cursor.fetchall()]

    print(f"Used keys: {used_keys}")

# Example usage
insert_and_retrieve_keys(5)

# Close the database connection
conn.close()

