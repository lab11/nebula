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
RUN_EXPRESS = True
RUN_PLAINTEXT = False
RUN_RAMP_WL = False
RUN_RAMP_EXPRESS = False

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
    n_mules = 100
    batch_size = 100
    existing_rows = 1000000

    # read dummy batchy data csv
    #  mule_id, batch_time
    dummy_csv = pd.read_csv('../simulation/probabilistic_routing/prob_data/random_uploads/vary_mules/{}_mule_dummy.csv'.format(n_mules), skiprows=2)

    dummy_csv.sort_values(["batch_time"],axis=0,inplace=True)
    print(dummy_csv.head(10))


    EX_SERVER_A = '34.205.45.52:4442'
    EX_SERVER_B = '18.209.20.193:4443'

    print('--- express realtime batch run w/ {} mules ---'.format(n_mules))

    express_procs = []
    for i in range(n_mules):
        express_procs.append(subprocess.Popen([
            '../express/client', 
            '-dataSize', '128',
            '-leaderIP', EX_SERVER_A,
            '-followerIP', EX_SERVER_B,
            '-numExistingRows', str(existing_rows),
            '-numThreads', '1',
            '-processId', str(i)
        ],stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=True))

    time.sleep(3)

    prev_time = 0.0

    time_to_wait = dummy_csv.iloc[0]['batch_time'] 
    print('  waiting {} s for next batch'.format(time_to_wait))
    time.sleep(time_to_wait)

    for idx, row in dummy_csv.iterrows():

        mule_id, curr_time = int(row['mule_id']), row['batch_time']

        print('waiting {} s for next batch'.format(curr_time - prev_time))
        time.sleep(curr_time - prev_time)

        print('\n\nprocessing mule {} batch at time {} s...'.format(mule_id, curr_time))
        
        for i in range(batch_size):
            # NOTE: express expects hex string payloads, so all a's work but not random strings
            express_procs[mule_id].stdin.write('1 {} {}\n'.format(mule_id, test_data))
            express_procs[mule_id].stdin.flush()

        prev_time = curr_time  

        
    # stop mules
    for e in express_procs:
        e.stdin.write('2\n')
        e.stdin.flush()


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
            '-numExistingRows', '100000',
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

        express.wait()

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
