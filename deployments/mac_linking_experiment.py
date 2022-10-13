from builtins import breakpoint
import multiprocessing as mp
import pandas.testing as pdt
from dateutil import parser
import os
import numpy as np
import pandas as pd
import threading
from tqdm import tqdm
import pdb
import re
from hashlib import sha256
import glob
import argparse

import pickle5 as pickle
from matplotlib import pyplot as plt
from scipy import signal

# Puts all the time and rssi values for each unique mac_hash into lists.
def get_log_df(data_path):
    # Load in our data
    dfBoi = pd.read_csv(data_path)
    
    # It looks like there's a jump in time from before time is synched,
    # so let's just delete the first few observations with bad times
    min_time = 1657100000000
    dfBoi = dfBoi.loc[dfBoi['ts'] > min_time]

    print(dfBoi['ts'].head)
    timeJump = max(np.diff(dfBoi['ts']))
    timeJumpIndex = list(np.diff(dfBoi['ts'])).index(timeJump)
    print("the time jump is ")
    print(timeJump)

    print("the time jump index is ")
    print(timeJumpIndex)

    # Removing the large time jump from the data 
    dfBoi.loc[(dfBoi.index > timeJumpIndex),'ts'] -= timeJump 
    
    # Aggregate time and rssi values together.
    dfBoi = dfBoi.sort_values(['ts'])
    dfComp = dfBoi.groupby('mac_hash', as_index=False).agg(list)
    
    return dfComp

# Read a pickle to a dataframe.
def read_pickle_to_df(aPickle):
    with open(aPickle, 'rb') as fh:
        df = pickle.load(fh)
        return df

# Identify all the interactions and append them to a list.
def calculate_interactions(df, maxgap=10.1, mincon=2.5, col='ts'):

  all_intervals = []
  min_time = np.inf
  max_time = -np.inf

  for i in tqdm(range(len(df))):
    macID = df.iloc[i]['mac_hash']

    smalldf = pd.DataFrame({'time':df.iloc[i][col], 'rssi':df.iloc[i]['rssi']})
    smalldf = smalldf.sort_values('time')
    smalldf['iit'] = smalldf.time.diff()

    possible_gaps = np.where(smalldf.iit > maxgap)[0]
    intervals = []
    start_time = smalldf.time[0]

    for next_index in possible_gaps:
      end_time = smalldf.time[next_index-1] 
      if end_time - start_time > mincon:
        intervals.append([start_time, end_time])
      start_time = smalldf.time[next_index]
    
    # get the last interaction as well
    end_time = smalldf.time[len(smalldf.time)-1]
    if end_time - start_time > mincon:
      intervals.append([start_time, end_time])
      if start_time < min_time:
        min_time = start_time 
      if end_time > max_time:
        max_time = end_time 

    all_intervals.append(intervals)

  df['interactions'] = all_intervals

  return min_time, max_time

# get the mac hashes that the nRF generated
def ground_truth(df):
  #The macs we made 
  init_mac = b'0xc098e5490000'
  ground_truth_hash = []
  len_ground_truth = len(dfBoi)
  
  for i in range(40):
    our_mac_hash = sha256()
    init_mac_string = str(init_mac)
    init_mac_forhash = bytes(init_mac_string[4:6]+':'+init_mac_string[6:8]+':'+init_mac_string[8:10]+':'+init_mac_string[10:12]+':'+init_mac_string[12:14]+':'+init_mac_string[14:16] , 'ascii')
    our_mac_hash.update(init_mac_forhash)
    hash_hex = our_mac_hash.hexdigest()
    ground_truth_hash.append(hash_hex)
    init_mac_int = int(init_mac, 16)
    init_mac_int += 1 
    init_mac = bytes(hex(init_mac_int),'ascii')
 
  return ground_truth_hash


#######################################################

if __name__ == '__main__':
    # Parse out our arguments.
    aParser = argparse.ArgumentParser(description='Generates interaction figures from raw log files collected by a Noble BLE sniffer.')
    ## For reading and processing log files.
    aParser.add_argument('--logs_dir', type=str, default='./data', help='Path to a directory of directories of log files.')
    aParser.add_argument('--pkls_dir', type=str, default='./pkls', help='Path to a directory where we store processed log files as pickles.')
    # For generating interactions with seen BLE devices.
    aParser.add_argument('--oui_list', type=str, default='./wireshark_oui_list.txt', help='Path to a file with known OUI prefixes.')
    aParser.add_argument('--max_gap', type=float, default=10.1, help='Maximum time (seconds) between advertisements before we consider it another interaction.')
    aParser.add_argument('--min_con', type=float, default=2.5, help='Minimum considered connection duration (seconds).')
    aParser.add_argument('--figs_dir', type=str, default='./figs', help='Path to a directory where we store the generated figures.')

    args = aParser.parse_args()

    # Create our pickles and figures directories if they do not exist.
    os.makedirs(args.pkls_dir, exist_ok=True)
    os.makedirs(args.figs_dir, exist_ok=True)

    # Crawl through a directory of log files and process them into individual pickles.
    for aLog in glob.glob('{}/*.log'.format(args.logs_dir)):
        # Ignore this log file if we have already pickled it.
        location = os.path.splitext(os.path.basename(aLog))[0]
        pkl_file = '{}/{}.pkl'.format(args.pkls_dir, location)
        if os.path.exists(pkl_file):
            continue
        
        # Aggregate the log file and pickle it.
        df = get_log_df(aLog)
        df.to_pickle(pkl_file)

    # Crawl through the pickles and generates RSSI Mac-linking figures for each pickle.
    for aPickle in glob.glob('{}/*.pkl'.format(args.pkls_dir)):
        # Read in our pickle.
        dfBoi = read_pickle_to_df(aPickle)
        location = aPickle.split('/')[-1].split('.')[0]

        # Get characteristics of each trace 
        mean_rssi = [np.mean(rssi_list) for rssi_list in dfBoi['rssi'].values]
        start_time = [time_list[0] for time_list in dfBoi['ts'].values]
        end_time = [time_list[-1] for time_list in dfBoi['ts'].values]
        
        # TODO: add frequency calculation and see if that helps 
        #average_frequency = 
        
        #print(dfBoi.shape)
        dfBoi.insert(2,'rssi_mean', mean_rssi)
        dfBoi.insert(3,'start_time', start_time)
        dfBoi.insert(4,'end_time', end_time)
        #print(dfBoi.shape)
        #dfBoi.insert(5,'frequency', len(dfBoi['rssi'].values) / dfBoi['ts'].values[-1] - dfBoi['ts'].values[0])

        # Get the ground truth mac hashes 
        ground_truth_hash = ground_truth(dfBoi)
        ground_truth_set = set(ground_truth_hash)
        df_hash_set = set(dfBoi['mac_hash'].values)

        # Get intersect of hashes and ground truth df
        intersect_hashes = ground_truth_set.intersection(df_hash_set)

        # Colors for each MAC 
        colors = ['b', 'g', 'r', 'c', 'm', 'y']
        numColors = len(colors)

        # Get timing information
        minTime = min(dfBoi.iloc[0]['ts'])
        maxTime = max(dfBoi.iloc[0]['ts'])
        for i in tqdm(range(len(dfBoi))):
            minTimeTemp = min(dfBoi.iloc[i]['ts'])
            maxTimeTemp = max(dfBoi.iloc[i]['ts'])
            if minTimeTemp < minTime:
                minTime = minTimeTemp
            if maxTimeTemp > maxTime:
                maxTime = maxTimeTemp
        
        maxTimeSec = (maxTime - minTime)/1000
        maxTimeMin = maxTimeSec / 60

        # Ground truth fig
        fig = plt.figure(figsize=(20,15))
        plt.xlabel('Time (hr)')
        plt.ylabel('RSSI')
        plt.ylim(-100,-10)
        plt.title('Ground Truth MACs RSSI vs. Time (hr)')

        # Plot ground truth 
        for i in tqdm(range(len(dfBoi))):
          macID = dfBoi.iloc[i]['mac_hash']
          if (macID in intersect_hashes):
            rssi_list = dfBoi.iloc[i]['rssi']
            time = dfBoi.iloc[i]['ts']
            timeMS = [x - minTime for x in time]
            timeSec = [x / 1000 for x in timeMS]
            timeMin = [x / 60 for x in timeSec]
            timeHour = [x / 60 for x in timeMin]

            b,a = signal.butter(8,0.025)
            rssi_filtered = signal.filtfilt(b,a,rssi_list,padlen=28)
            plt.plot(timeHour,rssi_filtered)
        
        plt.savefig("{}/{}_test_rssi_gt.png".format(args.figs_dir, location))
        plt.close()

        # Print the columns / figure info
        print(list(dfBoi.columns.values))
        print(aPickle)
        print("Generate a new figure to show all RSSI vs. time")

        # Plot all RSSI values 
        fig = plt.figure(figsize=(20,15))
        plt.xlabel('Time (hr)')
        plt.ylabel('RSSI')
        plt.ylim(-100,-10)
        plt.title('RSSI vs. Time')

        # Sort by start time 
        dfBoi = dfBoi.sort_values(['start_time'])
        dfBoi = dfBoi.reset_index(drop=True)

        # Initialize characteristics to first MAC characteristics 
        last_macID = dfBoi.iloc[0]['mac_hash']
        last_mac_mean = np.mean(dfBoi.iloc[0]['rssi'])
        last_mac_time = dfBoi.iloc[0]['start_time']
        last_mac_end_time = dfBoi.iloc[0]['end_time']

        # Save the mac links that were found 
        dfMACLinks = pd.DataFrame()

        # Iterate through all the hashes.
        for i in tqdm(range(len(dfBoi))):
            macID = dfBoi.iloc[i]['mac_hash']
            rssi_list = dfBoi.iloc[i]['rssi']
            time = dfBoi.iloc[i]['ts']
            timeMS = [x - minTime for x in time]
            timeSec = [x / 1000 for x in timeMS]
            timeMin = [x / 60 for x in timeSec]
            timeHour = [x / 60 for x in timeMin]

            exposureTimeMin = timeMin[-1]-timeMin[0]
            #if ((len(rssi_list) > 50) & (exposureTimeMin < 30) & (exposureTimeMin > 8)):
        
            variation = np.std(rssi_list)

            # Check for next possible MAC if not the last 10 elements
            #if (i != (len(dfBoi)-1) ):
            #  current_end_time = dfBoi.iloc[i]['end_time']
            #  current_time_length = dfBoi.iloc[i]['end_time'] - dfBoi.iloc[i]['start_time']
            #  window_length = 100000 # amount of seconds to check into future
              
            #  dfWindow = dfBoi[ (dfBoi['start_time'] > current_end_time) & (dfBoi['start_time'] < current_end_time+window_length)]
            #  # TODO only add to dfWindow if length and meanrssi is high enough
            #  dfWindow = dfWindow[dfWindow['rssi_mean']>-50]
            
            #  for j in range(len(dfWindow)):
            #    next_time_length = dfWindow.iloc[j]['end_time'] - dfWindow.iloc[j]['start_time']
            #    next_rssi_mean = dfWindow.iloc[j]['rssi_mean']
            #    next_rssi_list = dfWindow.iloc[j]['rssi']

                #if ( abs(next_time_length - current_time_length) < 10000):
                #  if (len(next_rssi_list) > 20):

            #    if ((next_rssi_mean > -60) & len(next_rssi_list) > 30):    
                  #if ( ((len(next_rssi_list) - len(rssi_list)) < 50) & len(next_rssi_list) > 20):
            #        print('yay found one')
                    #print(dfWindow.iloc[j]) # TODO: definitely change this...
            #        dfMACLinks = dfMACLinks.append(dfWindow.iloc[j])

            # FILTERED CODE 
            if ((np.mean(rssi_list) > -60) and (len(rssi_list) > 30) ): 
              b,a = signal.butter(8,0.025)
              rssi_filtered = signal.filtfilt(b,a,rssi_list,padlen=28)
              plt.plot(timeHour,rssi_filtered)

            # UNFILTERED CODE
            #plt.plot(timeHour,rssi_list)

        print("MAC Links", dfMACLinks)

        plt.savefig("{}/{}_test_rssi.png".format(args.figs_dir, location))
        plt.close()


        # plot the MAC Links 
        dfMACLinks = dfMACLinks.reset_index(drop=True)
        fig = plt.figure(figsize=(20,15))
        plt.xlabel('Time (hr)')
        plt.ylabel('RSSI')
        plt.ylim(-100,-10)
        plt.title('Found MAC Links RSSI vs. Time (hr)')

        # Plot ground truth 
        if len(dfMACLinks) > 5:
          for i in tqdm(range(len(dfMACLinks))):
            macID = dfMACLinks.iloc[i]['mac_hash']
            time = dfMACLinks.iloc[i]['ts']
            timeMS = [x - minTime for x in time]
            timeSec = [x / 1000 for x in timeMS]
            timeMin = [x / 60 for x in timeSec]
            timeHour = [x / 60 for x in timeMin]
            rssi_list = dfMACLinks.iloc[i]['rssi']

            b,a = signal.butter(8,0.025)
            #rssi_filtered = signal.filtfilt(b,a,rssi_list,padlen=28)
            print("ploting", rssi_list)
            plt.plot(timeHour,rssi_list)
          
          plt.savefig("{}/{}_test_rssi_ml.png".format(args.figs_dir, location))
          plt.close()



#timeMS[timeMS>]
#print(len(timeMSWarped))
#timeMSWarpedArr = np.array(timeMSWarped)
#timeMSecArr = np.where(timeMSWarpedArr>73352082,timeMSWarpedArr-timeWarp,timeMSWarpedArr)
#timeMS = np.ndarray.tolist(timeMSecArr)
#timeMS = [x - timeWarp for x in timeMSWarped if (x > 1137)]
#print(len(timeMSec))
#print(timeMSec)

            #if (variation > 5) & (variation < 8):
            #    if (timeSec[0] > 3) & (timeMin[-1] < 57):
            #if ((len(rssi_list) > 50) & (exposureTimeMin < 30) & (exposureTimeMin > 10) & (variation < 6)):
            #  b,a = signal.butter(8,0.015)
                      #print(timeMin[0])
                      #print(len(rssi_list))
                      #print(exposureTimeMin)
                      #print(variation)
            #  rssi_filtered = signal.filtfilt(b,a,rssi_list,padlen=28)

            #print("time",time)
            #print("time hr",timeHour)
            #print("rssis", rssi_list)
            #print("mac hash", macID)
            #print("mac",dfBoi.iloc[i]['mac'])



