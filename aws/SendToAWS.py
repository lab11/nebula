import os
import csv
import pandas as pd
import json
import sys
import time
import numpy as np
from multiprocessing import Pool

#select True to run express,plaintext,or workload 
RUN_EXPRESS = False
RUN_PLAINTEXT = True
RUN_RAMP_WL = False

#parallel processing function
def parallel_mqtt(msg):
    command = "python3 GalaxyAWS.py --topic topic/{} --root-ca ~/certs/Amazon-root-CA-1.pem --cert ~/certs/device.pem.crt --key ~/certs/private.pem.key --endpoint a3gshqjfftdu7n-ats.iot.us-west-1.amazonaws.com --message '{}' --count 1".format(msg['mule_id'],json.dumps(msg))
    #print(command)
    #command = "python3 GalaxyAWS.py --topic topic_1 --root-ca ~/certs/Amazon-root-CA-1.pem --cert ~/certs/device.pem.crt --key ~/certs/private.pem.key --endpoint a3gshqjfftdu7n-ats.iot.us-west-1.amazonaws.com --message '{}' --count 1".format(json.dumps(msg))
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

if RUN_PLAINTEXT == True:
    schedule_csv.sort_values(["pickup_time"],axis=0,inplace=True)
    #print(schedule_csv.head(50))
    
    #get the unique sample_times
    unique_sample_times = schedule_csv.sample_time.unique()
    #print(unique_sample_times)

    #iterate through the sample times 
    for sample_time in unique_sample_times:
        #get a new df and unique mule_ids
        data_to_send = schedule_csv.loc[schedule_csv['sample_time'] == sample_time]
        unique_mule_ids = data_to_send.mule_id.unique()

        #initialize msg array and loop through mule ids to populate msgs 
        msgs = []
        for mule_id in unique_mule_ids:
            message_data = data_to_send.loc[data_to_send['mule_id'] == mule_id]
            msg = {
                "mule_id":int(mule_id),
                "sensor_id":message_data['sensor_id'].to_numpy().tolist(),
                "data":[25,25,25]
            }          
            msgs.append(msg)
         
        #truncate messages because we only have 96 cpus! 
        msgs = msgs[0:90]

        #start timer
        start_time = time.time()
        
        #send msg array in parallel for sample_time
        pool = Pool()
        pool.map(parallel_mqtt,msgs)
        pool.close()

        end_time = time.time()
        print(end_time-start_time)

elif RUN_EXPRESS == True:
    schedule_csv.sort_values(["batch_time"],axis=0,inplace=True)
    print(schedule_csv.head(10))

    #get the unique pickup time
    unique_pickup_time = schedule_csv.pickup_time.unique()

    for sample_time in unique_pickup_time:
        #get a new df and unique mule_ids
        data_to_send = schedule_csv.loc[schedule_csv['sample_time'] == sample_time]
        unique_mule_ids = data_to_send.mule_id.unique()

        #initialize msg array and loop through mule ids to populate msgs 
        msgs = []
        for mule_id in unique_mule_ids:
            message_data = data_to_send.loc[data_to_send['mule_id'] == mule_id]
            msg = {
                "mule_id":int(mule_id),
                "sensor_id":message_data['sensor_id'].to_numpy().tolist(),
                "data":[25,25,25]
            }          
            msgs.append(msg)

    #TODO call Express binary with data 

elif RUN_RAMP_WL == True:
    #loop through to make message arrays of increasing size
    for i in range(9):
        number_messages = (i+1)*10
        
        #initialize message and message array 
        msg = {
            "mule_id":0,
            "sensor_id":0,
            "data":[25,25,25]
        }
        msgs = [msg for i in range(number_messages)]

        #start timer 
        start_time = time.time()

        #send messages in parallel
        pool = Pool()
        pool.map(parallel_mqtt,msgs)
        pool.close()

        end_time = time.time()

        #TODO write to csv 
        print(number_messages)
        print(end_time-start_time)
