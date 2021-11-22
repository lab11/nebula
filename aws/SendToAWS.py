import os
import csv
import json
import sys
import time

start_time = time.time()
data_from_csv = []
with open('nRf_data.csv', 'r') as csvfile:
    csvreader = csv.reader(csvfile)
    for i,row in enumerate(csvreader):
        #print(row)
        if (i >= 1):
            data_from_csv.append(row[1])

#print(data_from_csv)

data = {
    "sensor_id":5,
    "mule_id":0,
    "data":int(data_from_csv[0]) #eventually this should be changed to handle multiple data packets
}

print("size of just data ", sys.getsizeof(int(data_from_csv[0])))

#send data over
command = "python3 GalaxyAWS.py --topic topic_1 --root-ca ~/certs/Amazon-root-CA-1.pem --cert ~/certs/device.pem.crt --key ~/certs/private.pem.key --endpoint a3gshqjfftdu7n-ats.iot.us-west-1.amazonaws.com --message '{}' --count 1".format(json.dumps(data))
print(command)
os.system(command)

end_time = time.time()

print("Total time to get data from Pi to AWS ", end_time-start_time)