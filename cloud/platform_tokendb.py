import sqlite3
import threading

DEBUG = True

class StringSet:
    def __init__(self):
        self.conn = sqlite3.connect("strings.db")
        self.lock = threading.Lock()
        self._create_table()

    def _create_table(self):
        cursor = self.conn.cursor()
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS strings (
                value TEXT PRIMARY KEY
            )
        """)
        self.conn.commit()

    def add_new_elements(self, new_strings):
        with self.lock:
            cursor = self.conn.cursor()
            initial_size = cursor.execute("SELECT COUNT(*) FROM strings").fetchone()[0]

            for string in new_strings:
                try:
                    cursor.execute("INSERT INTO strings (value) VALUES (?)", (string,))
                except sqlite3.IntegrityError:
                    pass

            self.conn.commit()

            final_size = cursor.execute("SELECT COUNT(*) FROM strings").fetchone()[0]
            return final_size - initial_size

    def close(self):
        self.conn.close()

if DEBUG:

    string_set = StringSet()

    # Add new strings to the set and print the number of new elements added
    new_strings = ['hello', 'world', 'foo', 'bar']
    print(string_set.add_new_elements(new_strings))  # Output: 4

    # Add some new strings and some existing strings
    new_strings = ['hello', 'world', 'python', 'example']
    print(string_set.add_new_elements(new_strings))  # Output: 2

    string_set.close()

