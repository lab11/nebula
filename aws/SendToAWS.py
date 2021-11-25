import os
import csv
import pandas as pd
import json
import sys
import time
from multiprocessing import Pool

#select True to run express code, select False to run plaintext 
RUN_EXPRESS = False

#parallel processing function
def parallel_mqtt(msg):
    print(msg['mule_id'])
    print(msg)
    command = "python3 GalaxyAWS.py --topic topic/{} --root-ca ~/certs/Amazon-root-CA-1.pem --cert ~/certs/device.pem.crt --key ~/certs/private.pem.key --endpoint a3gshqjfftdu7n-ats.iot.us-west-1.amazonaws.com --message '{}' --count 1".format(msg['mule_id'],json.dumps(msg))
    print(command)
    os.system(command)


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

#import schedule csv 
#header:'sensor_id', 'mule_id', 'sample_time', 'pickup_time', 'batch_time', 'data_length'
schedule_csv = pd.read_csv('../simulation/probabilistic_routing/prob_data/schedule.csv', skiprows=3)


#start timing
start_time = time.time()

if RUN_EXPRESS == False:
    schedule_csv.sort_values(["pickup_time"],axis=0,inplace=True)
    print(schedule_csv.head(50))
    #TODO send messages in parallel for each pickup time
    #each mule_id within a pickup time makes own data packet with sensor ids and data 
elif RUN_EXPRESS == True:
    schedule_csv.sort_values(["batch_time"],axis=0,inplace=True)
    print(schedule_csv.head(10))
    #TODO call Express binary with data 

#exit()
'''
schedule_csv = []
with open('../simulation/probabilistic_routing/prob_data/schedule.csv') as csvfile:
    csvreader = csv.reader(csvfile)
    for i,row in enumerate(csvreader):
        if (i < 10) & (i >= 3):
            schedule_csv.append(row)
print(schedule_csv)
print(schedule_csv[0])
print(schedule_csv[1][0])
exit()

#prob/schedule
'''
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

