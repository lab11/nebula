from pymemcache.client import base

# Run `sudo systemctl restart memcached` before running the next line. 
client = base.Client(('localhost', 11211))
client.set('some_key', 'some value')
print(client.get('some_key'))
