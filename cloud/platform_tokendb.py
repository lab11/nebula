import redis

class KeyValueDatabase:
    def __init__(self, host='localhost', port=6379, db=0):
        self.redis_client = redis.StrictRedis(host=host, port=port, db=db)

        # Initialize the database and set counts to 0
        for key in self.redis_client.scan_iter('*'):
            self.redis_client.set(key, 0)

    def increment_count(self, mule_id):
        self.redis_client.incr(mule_id)

    def batch_increment_counts(self, mule_ids):
        for mule_id in mule_ids:
            self.increment_count(mule_id)

    def get_counts(self):
        counts = {}
        for key in self.redis_client.scan_iter('*'):
            counts[key.decode()] = int(self.redis_client.get(key))
        return counts

if __name__ == '__main__':
    db = KeyValueDatabase()

    # Increment counts for mule_ids
    mule_ids = ['mule_1', 'mule_2', 'mule_3']
    db.batch_increment_counts(mule_ids)

    # Retrieve the current counts
    print(db.get_counts())

