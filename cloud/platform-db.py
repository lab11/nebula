import sqlite3
from concurrent.futures import ProcessPoolExecutor

class KeyValueDatabase:
    def __init__(self, db_name='mules.db'):
        self.db_name = db_name
        self._init_db()

    def _init_db(self):
        with sqlite3.connect(self.db_name) as conn:
            conn.execute('''CREATE TABLE IF NOT EXISTS mules (
                                mule_id TEXT PRIMARY KEY,
                                count INTEGER NOT NULL
                            )''')
            conn.commit()

    def increment_count(self, mule_id):
        with sqlite3.connect(self.db_name) as conn:
            conn.execute('''INSERT OR IGNORE INTO mules (mule_id, count)
                            VALUES (?, 0)''', (mule_id,))
            conn.execute('''UPDATE mules
                            SET count = count + 1
                            WHERE mule_id = ?''', (mule_id,))
            conn.commit()

    def batch_increment_counts(self, mule_ids):
        with ProcessPoolExecutor(max_workers=32) as executor:
            executor.map(self.increment_count, mule_ids)

    def get_counts(self):
        with sqlite3.connect(self.db_name) as conn:
            cursor = conn.execute('''SELECT mule_id, count
                                     FROM mules''')
            return {row[0]: row[1] for row in cursor}

if __name__ == '__main__':
    db = KeyValueDatabase()

    # Increment counts for mule_ids
    mule_ids = ['mule_1', 'mule_2', 'mule_3', 'mule_2']
    db.batch_increment_counts(mule_ids)

    # Retrieve the current counts
    print(db.get_counts())

