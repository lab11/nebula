import os
import csv
import json
import sys
import time
from multiprocessing import Pool

#parallel processing function
def parallel_mqtt(msg):
    print(msg['mule_id'])
    print(msg)
    command = "python3 GalaxyAWS.py --topic topic/{} --root-ca ~/certs/Amazon-root-CA-1.pem --cert ~/certs/device.pem.crt --key ~/certs/private.pem.key --endpoint a3gshqjfftdu7n-ats.iot.us-west-1.amazonaws.com --message '{}' --count 1".format(msg['mule_id'],json.dumps(msg))
    print(command)
    os.system(command)

#start timing 
start_time = time.time()

#pull in data from nRF board 
data_from_csv = []
with open('nRf_data.csv', 'r') as csvfile:
    csvreader = csv.reader(csvfile)
    for i,row in enumerate(csvreader):
        #print(row)
        if (i >= 1):
            data_from_csv.append(row[1])

#initialize test data (bytes didn't work, so deal with that later)
test_data = os.urandom(128)

#TODO import interactions csv 

#TODO organize data by mules into messages 


#TODO if plaintext sort by time 

#TODO else express sort by time sent 


sensor_id = 0
mule_id = 1

#Testing data
data1 = {
    "sensor_id":sensor_id,
    "mule_id":0,
    "data":50 #eventually this should be changed to handle multiple data packets
}

data2 = {
    "sensor_id":sensor_id,
    "mule_id":0,
    "data":int(data_from_csv[0]) #eventually this should be changed to handle multiple data packets
}

data3 = {
    "sensor_id":sensor_id,
    "mule_id":2,
    "data":int(data_from_csv[0]) #eventually this should be changed to handle multiple data packets
}

#send messages in parallel TODO replace messages with Alvin's or other work load  
msgs = [data1,data2,data3]
pool = Pool(processes=3)
pool.map(parallel_mqtt,msgs)
pool.close()

#end time
end_time = time.time()
#print out the total time
print("Total time to get data from EC2 to AWS ", end_time-start_time)

