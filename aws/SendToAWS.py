import os
import csv

data = ''
with open('nRf_data.csv', 'r') as csvfile:
    csvreader = csv.reader(csvfile)
    for row in csvreader:
        print(row)
        data += str(row)

print(data)


#send data over
os.system('python3 ../../../../../../aws-iot-device-sdk-python-v2/samples/pubsub.py --topic topic_1 --root-ca ~/certs/Amazon-root-CA-1.pem --cert ~/certs/device.pem.crt --key ~/certs/private.pem.key --endpoint a3gshqjfftdu7n-ats.iot.us-west-1.amazonaws.com --message "{}" --count 3'.format(str(data)))