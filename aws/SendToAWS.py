import os
import csv
import json

data_from_csv = []
with open('nRf_data.csv', 'r') as csvfile:
    csvreader = csv.reader(csvfile)
    for i,row in enumerate(csvreader):
        #print(row)
        if (i >= 1):
            data_from_csv.append(row[1])

#print(data_from_csv)

data = {
    "sensor_id":0,
    "mule_id":0,
    "data":int(data_from_csv[0]) #eventually this should be changed to handle multiple data packets
}

#send data over
command = "python3 GalaxyAWS.py --topic topic_1 --root-ca ~/certs/Amazon-root-CA-1.pem --cert ~/certs/device.pem.crt --key ~/certs/private.pem.key --endpoint a3gshqjfftdu7n-ats.iot.us-west-1.amazonaws.com --message '{}' --count 3".format(json.dumps(data))
print(command)
os.system(command)