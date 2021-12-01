import os
import csv
import pandas as pd
import json
import sys
import time
import subprocess
import numpy as np
from multiprocessing import Pool

#select True to run express,plaintext,or workload 
RUN_EXPRESS = False 
RUN_PLAINTEXT = False
RUN_RAMP_WL = False
RUN_RAMP_EXPRESS = True

#parallel processing function
def parallel_mqtt(msg):
    command = "python3 GalaxyAWS.py --topic topic/{} --root-ca ~/certs/Amazon-root-CA-1.pem --cert ~/certs/device.pem.crt --key ~/certs/private.pem.key --endpoint a3gshqjfftdu7n-ats.iot.us-west-1.amazonaws.com --message '{}' --count 1".format(msg['mule_id'],json.dumps(msg))
    #print(command) #command = "python3 GalaxyAWS.py --topic topic_1 --root-ca ~/certs/Amazon-root-CA-1.pem --cert ~/certs/device.pem.crt --key ~/certs/private.pem.key --endpoint a3gshqjfftdu7n-ats.iot.us-west-1.amazonaws.com --message '{}' --count 1".format(json.dumps(msg))
    os.system(command)

#pull in data from nRF board 
data_from_csv = []
with open('nRf_data.csv', 'r') as csvfile:
    csvreader = csv.reader(csvfile)
    for i,row in enumerate(csvreader):
        #print(row)
        if (i >= 1):
            data_from_csv.append(row[1])

#initialize test data of 79 string characters which is 128 bytes
test_data = "a"*78

#import schedule csv 
#header:'sensor_id', 'mule_id', 'sample_time', 'pickup_time', 'batch_time', 'data_length'
schedule_csv = pd.read_csv('../simulation/probabilistic_routing/prob_data/continual_motion/schedule.csv', skiprows=3)

if RUN_PLAINTEXT == True:
    schedule_csv.sort_values(["pickup_time"],axis=0,inplace=True)
    
    #get the unique pickup_times
    unique_pickup_times = schedule_csv.pickup_time.unique()

    #iterate through the sample times 
    for pickup_time in unique_pickup_times:
        #get a new df and unique mule_ids
        data_to_send = schedule_csv.loc[schedule_csv['pickup_time'] == pickup_time]
        unique_mule_ids = data_to_send.mule_id.unique()

        #initialize msg array and loop through mule ids to populate msgs 
        msgs = []
        for mule_id in unique_mule_ids:
            message_data = data_to_send.loc[data_to_send['mule_id'] == mule_id]
            msg = {
                "mule_id":int(mule_id),
                "sensor_id":message_data['sensor_id'].to_numpy().tolist(),
                "data":test_data
            }          
            msgs.append(msg)
         
        #truncate messages to limit to under 96 CPUs 
        #msgs = msgs[0:90]

        #start timer
        start_time = time.time()
        
        #send msg array in parallel for sample_time
        pool = Pool()
        pool.map(parallel_mqtt,msgs)
        pool.close()

        end_time = time.time()
        print(end_time-start_time)

        number_messages = len(msgs)

        #open a csv to write data to 
        with open('latency_tp_pt_schedule.csv', mode='a') as csvfile:
            csvwriter = csv.writer(csvfile,delimiter=',')
            csvwriter.writerow([number_messages,end_time-start_time,(number_messages/(end_time-start_time))])

elif RUN_EXPRESS == True:
    schedule_csv.sort_values(["batch_time"],axis=0,inplace=True)
    print(schedule_csv.head(10))

    EX_SERVER_A = '34.205.45.52:4442'
    EX_SERVER_B = '18.209.20.193:4443'

    express = subprocess.Popen([
        '../express/client', 
        '-dataSize', '128',
        '-leaderIP', EX_SERVER_A,
        '-followerIP', EX_SERVER_B,
        '-numExistingRows', '1000000',
        '-numThreads', '95'
    ],stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True)

    # sleep to give express a chance to start up
    time.sleep(1)

    # add a new row for every mule id
    '''
    print('adding a new row for every mule id')
    mule_ids = schedule_csv.mule_id.unique()
    mule_id_map = {}
    for i, mule_id in enumerate(mule_ids):
        # send signal for new row, we'll use index in the list as the local row index
        express.stdin.write('0\n') 
        express.stdin.flush()
        # TODO read response but we don't need to right now
        mule_id_map[mule_id] = i
    '''

    #get the unique pickup time
    unique_pickup_time = schedule_csv.pickup_time.unique()

    #time.sleep(3)

    print('starting transmissions')
    for pickup_time in unique_pickup_time:
        #get a new df and unique mule_ids
        data_to_send = schedule_csv.loc[schedule_csv['pickup_time'] == pickup_time]
        unique_mule_ids = data_to_send.mule_id.unique()

        #initialize msg array and loop through mule ids to populate msgs 
        msgs = []
        for mule_id in unique_mule_ids:
            message_data = data_to_send.loc[data_to_send['mule_id'] == mule_id]
            msg = {
                "mule_id":int(mule_id),
                "sensor_id":message_data['sensor_id'].to_numpy().tolist(),
                "data":test_data
            }          
            msgs.append(msg)

        # NOTE: express expects hex string payloads, so all a's work but not random strings
        for m in msgs:
            express.stdin.write('1 {} {}\n'.format(mule_id_map[m['mule_id']], m['data']))
            express.stdin.flush()

    express.stdin.write('2\n')
    express.stdin.flush()

    print('finished transmissions')

elif RUN_RAMP_EXPRESS == True:

    EX_SERVER_A = '34.205.45.52:4442'
    EX_SERVER_B = '18.209.20.193:4443'

    for i in range(18):
        number_parallel_messages = (i+1) * 5

        print('--- express ramp w/ {} mules ---'.format(number_parallel_messages))

        express = subprocess.Popen([
            '../express/client', 
            '-dataSize', '128',
            '-leaderIP', EX_SERVER_A,
            '-followerIP', EX_SERVER_B,
            '-numExistingRows', '1000000',
            '-numThreads', str(number_parallel_messages)
        ],stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True)

        # sleep to give express a chance to start up
        time.sleep(1)

        #initialize message and message array 
        msg = {
            "mule_id":0,
            "sensor_id":0,
            "data":test_data
        }
        msgs = [msg for i in range(number_parallel_messages)]

        '''
        #open a csv to write data to 
        with open('latency.csv', mode='a') as csvfile:
            csvwriter = csv.writer(csvfile,delimiter=',')
            csvwriter.writerow([number_messages,end_time-start_time])
        '''

        # NOTE: express expects hex string payloads, so all a's work but not random strings
        for m in msgs:
            express.stdin.write('1 {} {}\n'.format(m['mule_id'], m['data']))
            express.stdin.flush()

        express.stdin.write('2\n')
        express.stdin.flush()

        while True:
            pass


elif RUN_RAMP_WL == True:
    #loop through to make message arrays of increasing size
    for i in range(18):
        number_messages = (i+1)*5
        
        #initialize message and message array 
        msg = {
            "mule_id":0,
            "sensor_id":0,
            "data":test_data
        }
        msgs = [msg for i in range(number_messages)]

        #start timer 
        start_time = time.time()

        #send messages in parallel
        pool = Pool()
        pool.map(parallel_mqtt,msgs)
        pool.close()

        end_time = time.time()

        print(number_messages)
        print(end_time-start_time)
        #open a csv to write data to 
        with open('latency.csv', mode='a') as csvfile:
            csvwriter = csv.writer(csvfile,delimiter=',')
            csvwriter.writerow([number_messages,end_time-start_time])
