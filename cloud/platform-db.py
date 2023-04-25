import sqlite3
from concurrent.futures import ProcessPoolExecutor

DEBUG = True

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

    def increment_count(self, mule_id, increment):
        with sqlite3.connect(self.db_name) as conn:
            conn.execute('''INSERT OR IGNORE INTO mules (mule_id, count)
                            VALUES (?, 0)''', (mule_id,))
            conn.execute('''UPDATE mules
                            SET count = count + ?
                            WHERE mule_id = ?''', (increment, mule_id))
            conn.commit()

    def batch_increment_counts(self, mule_id_increments):
        with ProcessPoolExecutor(max_workers=32) as executor:
            executor.map(lambda x: self.increment_count(x[0], x[1]), mule_id_increments)

    def get_counts(self):
        with sqlite3.connect(self.db_name) as conn:
            cursor = conn.execute('''SELECT mule_id, count
                                     FROM mules''')
            return {row[0]: row[1] for row in cursor}

if DEBUG:
    db = KeyValueDatabase()

    # Increment counts for mule_ids
    mule_id_increments = [('mule_1', 2), ('mule_2', 3), ('mule_3', 1)]
    db.batch_increment_counts(mule_id_increments)

    # Retrieve the current counts
    print(db.get_counts())

