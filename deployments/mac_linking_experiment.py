
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

pd.set_option('max_columns', None)

# Puts all the time and rssi values for each unique mac_hash into lists.
def get_log_df(data_path):
    # Load in our data
    dfBoi = pd.read_csv(data_path)
    
    # It looks like there's a jump in time from before time is synched,
    # so let's just delete the first few observations with bad times
    min_time = 1657100000000
    dfBoi = dfBoi.loc[dfBoi['ts'] > min_time]

    #print(dfBoi['ts'].head)
    timeJump = max(np.diff(dfBoi['ts']))
    timeJumpIndex = list(np.diff(dfBoi['ts'])).index(timeJump)
    #print("the time jump is ")
    #print(timeJump)

    #print("the time jump index is ")
    #print(timeJumpIndex)

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

# add characteristics for each trace (mean, stdev etc)
def add_characteristics(df):
  # Get characteristics of each trace 
  rssi_mean = [np.mean(rssi_list) for rssi_list in df['rssi'].values]
  rssi_stdev = [np.std(rssi_list) for rssi_list in df['rssi'].values]
  rssi_length = [len(rssi_list) for rssi_list in df['rssi'].values]
  start_time = [time_list[0] for time_list in df['ts'].values]
  end_time = [time_list[-1] for time_list in df['ts'].values]
  period = np.divide(rssi_length, np.subtract(end_time,start_time)*0.001) # in seconds
  frequency = 1 / period # in Hz
      
  # TODO: add frequency calculation and see if that helps 
  #average_frequency = 
      
  #print(dfBoi.shape)
  df.insert(2,'rssi_mean', rssi_mean)
  df.insert(3,'rssi_length', rssi_length)
  df.insert(4,'start_time', start_time)
  df.insert(5,'end_time', end_time)
  df.insert(6,'rssi_stdev', rssi_stdev)
  df.insert(7,'period', period)
  df.insert(8,'frequency', frequency)

  return df

# get min and max timing information
def get_timing(df):
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
  return minTime, maxTime

# plot the ground truth 
def plot_ground_truth(df, intersect_hashes, minTime, args, location):
  fig = plt.figure(figsize=(20,15))
  plt.xlabel('Time (hr)')
  plt.ylabel('RSSI')
  plt.ylim(-100,-10)
  plt.title('Ground Truth MACs RSSI vs. Time (hr)')

  df_gt = df[df['mac_hash'].isin(intersect_hashes)]

  # Plot ground truth 
  for i in tqdm(range(len(df))):
    macID = df.iloc[i]['mac_hash']
    if (macID in intersect_hashes):
      rssi_list = df.iloc[i]['rssi']
      time = df.iloc[i]['ts']
      timeMS = [x - minTime for x in time]
      timeSec = [x / 1000 for x in timeMS]
      timeMin = [x / 60 for x in timeSec]
      timeHour = [x / 60 for x in timeMin]

      b,a = signal.butter(8,0.025)
      rssi_filtered = signal.filtfilt(b,a,rssi_list,padlen=28)
      plt.plot(timeHour,rssi_filtered)
  
  plt.savefig("{}/{}_test_rssi_gt_filtered.png".format(args.figs_dir, location))
  plt.close()

  return df_gt

def plot_all(df, minTime, args, location):
  fig = plt.figure(figsize=(20,15))
  plt.xlabel('Time (hr)')
  plt.ylabel('RSSI')
  plt.ylim(-100,-10)
  plt.title('RSSI vs. Time')

    # Iterate through all the hashes.
  for i in tqdm(range(len(df))):
      macID = df.iloc[i]['mac_hash']
      rssi_list = df.iloc[i]['rssi']
      time = df.iloc[i]['ts']
      timeMS = [x - minTime for x in time]
      timeSec = [x / 1000 for x in timeMS]
      timeMin = [x / 60 for x in timeSec]
      timeHour = [x / 60 for x in timeMin]

      if ((np.mean(rssi_list) > -60) and (len(rssi_list) > 30) ): 
                  b,a = signal.butter(8,0.025)
                  rssi_filtered = signal.filtfilt(b,a,rssi_list,padlen=28)
                  plt.plot(timeHour,rssi_filtered)

  plt.savefig("{}/{}_test_rssi.png".format(args.figs_dir, location))
  plt.close()

 
def get_graph_huristics(df, gt_or_all):

  rssi_mean_list = df['rssi_mean'].values
  rssi_stdev_list = df['rssi_stdev'].values
  rssi_length_list = df['rssi_length'].values
  start_time_list = df['start_time'].values
  end_time_list = df['end_time'].values
  frequencies = df['frequency'].values
  time_list = (end_time_list - start_time_list) / (60*1000) #minutes 

  #time_list_filtered = [i < 0.5 for i in time_list]
  #frequencies_filtered = [i < 1000 for i in frequencies]

  fig, ax = plt.subplots(4, sharex=False, figsize=(20,15))
  ax[0].set_title('RSSI Mean')
  ax[1].set_title('Frequency (Hz)')
  ax[2].set_title('Time (ms)')
  ax[3].set_title('RSSI Stdev')
  ax[0].hist(rssi_mean_list, bins=100)
  ax[1].hist(frequencies, bins=100)
  ax[2].hist(time_list, bins=100)
  ax[3].hist(rssi_stdev_list, bins=100)

  plt.savefig("{}/{}_test_huristics_{}.png".format(args.figs_dir, location, gt_or_all))
  plt.close()


def link_macs(df):
  # Sort by start time 
  df = df.sort_values(['start_time'])
  df = df.reset_index(drop=True)

  # Initialize list of MACs that have been linked 
  dfMACLinks = pd.DataFrame()

  for i in tqdm(range(len(df))):
    # Get MAC characteristics
    macID = df.iloc[i]['mac_hash']
    mac_mean = df.iloc[i]['rssi_mean']
    mac_start_time = df.iloc[i]['start_time']
    mac_end_time = df.iloc[i]['end_time']
    mac_length = df.iloc[i]['rssi_length']
    mac_stdev = df.iloc[i]['rssi_stdev']
    mac_rssi_list = df.iloc[i]['rssi']
    mac_time = mac_end_time - mac_start_time
    mac_period = mac_time / mac_length
    window_length = 5 * mac_period  

    #dfMACLinks = dfMACLinks.append(df.iloc[i])
    
    # traces have to be at least 10 minutes long to check for a next link
    if (mac_time > 1000 * 60 * 10): 
      # Find all the MACs that are within the window and filter for length and mean
      dfWindow = df[ (df['start_time'] > mac_end_time) & (df['start_time'] < mac_end_time+window_length)]
      dfWindow = dfWindow[ (dfWindow['rssi_mean'] > -50)]
      dfWindow = dfWindow[ (dfWindow['rssi_length']) > 500]
      dfWindow = dfWindow[ (dfWindow['rssi_stdev']) < 8]

      # Look for candidate next MAC within the window
      dfWindow = dfWindow.sort_values(['start_time'])
      dfWindow = dfWindow.reset_index(drop=True) 

      #dfMACLinks = dfMACLinks.append(df.iloc[i])
      best_in_window = None
      last_match_score = 300 #experimentally found to be kind of a reasonable limit
      for j in range(len(dfWindow)):
        #print(len(dfWindow))
        if (dfWindow.iloc[j]['end_time'] - dfWindow.iloc[j]['start_time'] > 1000 * 60 * 10): #all candidates must be longer than 10min
            
          # Check the last 10 data points and their rssi mean, stdev, frequency, and length
          rssi_list = dfWindow.iloc[j]['rssi']
          mean_diff = abs(np.mean((mac_rssi_list[-5:]) - np.mean(rssi_list[0:5])))
          stdev_diff = abs(np.std((mac_rssi_list[-5:]) - np.std(rssi_list[0:5])))#abs(dfWindow.iloc[j]['rssi_stdev'] - mac_stdev)
          freq_diff = abs(dfWindow.iloc[j]['frequency'] - df.iloc[i]['frequency'])
          length_diff = (abs(dfWindow.iloc[j]['rssi_length'] - mac_length))

          match_score = mean_diff+stdev_diff+freq_diff+length_diff #TODO think about normalizing to a distribution
          #print(match_score)
          if (match_score < last_match_score):
            last_match_score = match_score
            best_in_window = dfWindow.iloc[j]
            #print(match_score)
      
      if (best_in_window is not None):
        dfMACLinks = dfMACLinks.append(best_in_window)
        dfMACLinks = dfMACLinks.append(df.iloc[i])      

  return dfMACLinks

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
        dfBoi = add_characteristics(dfBoi)

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
        minTime, maxTime = get_timing(dfBoi)
        maxTimeSec = (maxTime - minTime)/1000
        maxTimeMin = maxTimeSec / 60

        # Plot ground truth 
        df_gt = plot_ground_truth(dfBoi,intersect_hashes, minTime, args, location)

        # Plot all RSSI values 
        plot_all(dfBoi, minTime, args, location)

        # Plot huristics 
        get_graph_huristics(dfBoi, "all")
        get_graph_huristics(df_gt, "ground_truth")

        # Link MACs
        dfMACLinks = link_macs(dfBoi)

        # plot the MAC Links 
        dfMACLinks = dfMACLinks.reset_index(drop=True)
        fig = plt.figure(figsize=(20,15))
        plt.xlabel('Time (hr)')
        plt.ylabel('RSSI')
        plt.ylim(-100,-10)
        plt.title('Found MAC Links RSSI vs. Time (hr)')

        # Plot reconstructed ground truth 
        for i in tqdm(range(len(dfMACLinks))):
          rssi_list = dfMACLinks.iloc[i]['rssi']
          macID = dfMACLinks.iloc[i]['mac_hash']
          time = dfMACLinks.iloc[i]['ts']
          timeMS = [x - minTime for x in time]
          timeSec = [x / 1000 for x in timeMS]
          timeMin = [x / 60 for x in timeSec]
          timeHour = [x / 60 for x in timeMin]
          
          if ((np.mean(rssi_list) > -60) and (len(rssi_list) > 30) ): 
            b,a = signal.butter(8,0.025)
            rssi_filtered = signal.filtfilt(b,a,rssi_list,padlen=28)
            #print("ploting", rssi_list)
            plt.plot(timeHour,rssi_filtered)
          
        plt.savefig("{}/{}_test_rssi_ml.png".format(args.figs_dir, location))
        plt.close()


#TODO jetison the first and last traces? 

#TODO names / headers reread BLE doubt!!! 
#TODO stability on a small time scale
#TODO fix histograms 


### TRASH CODE BELOW ###


 # # Initialize characteristics to first MAC characteristics 
  # last_macID = df.iloc[0]['mac_hash']
  # last_mac_mean = df.iloc[0]['rssi_mean']
  # last_mac_start_time = df.iloc[0]['start_time']
  # last_mac_end_time = df.iloc[0]['end_time']
  # last_mac_length = df.iloc[0]['rssi_length']

        # # Sort by start time 
        # dfBoi = dfBoi.sort_values(['start_time'])
        # dfBoi = dfBoi.reset_index(drop=True)

        # # Initialize characteristics to first MAC characteristics 
        # last_macID = dfBoi.iloc[0]['mac_hash']
        # last_mac_mean = np.mean(dfBoi.iloc[0]['rssi'])
        # last_mac_time = dfBoi.iloc[0]['start_time']
        # last_mac_end_time = dfBoi.iloc[0]['end_time']

        # # Save the mac links that were found 
        # dfMACLinks = pd.DataFrame()

            #exposureTimeMin = timeMin[-1]-timeMin[0]
            #if ((len(rssi_list) > 50) & (exposureTimeMin < 30) & (exposureTimeMin > 8)):
        
            #variation = np.std(rssi_list)

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
            # if ((np.mean(rssi_list) > -60) and (len(rssi_list) > 30) ): 
            #   b,a = signal.butter(8,0.025)
            #   rssi_filtered = signal.filtfilt(b,a,rssi_list,padlen=28)
            #   plt.plot(timeHour,rssi_filtered)

            # UNFILTERED CODE
            #plt.plot(timeHour,rssi_list)

        #print("MAC Links", dfMACLinks)

        #plt.savefig("{}/{}_test_rssi.png".format(args.figs_dir, location))
        #plt.close()


                #print ("dfWindow: ", dfWindow.iloc[i]['mac_hash'])
        #if (dfWindow.iloc[j]['end_time'] - dfWindow.iloc[j]['start_time'] > 10000 * 60 * 5):
        #dfMACLinks = dfMACLinks.append(dfWindow.iloc[j])

      
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


            #
          
          
          # #TODO this sucks fix it 
          # # check lengths 
          # if ( (abs(dfWindow.iloc[j]['rssi_length'] - mac_length) < 300)):
          #   #check means
          #       if (abs(dfWindow.iloc[j]['rssi_mean'] - mac_mean) < 25):
          #          #check stdevs
          #          if (abs(dfWindow.iloc[j]['rssi_stdev'] - mac_stdev) < 5):
          #            #check frequencies
          #            if (abs(dfWindow.iloc[j]['frequency'] - df.iloc[i]['frequency']) < 1):
          #             if (best_in_window is None):
          #               best_in_window = dfWindow.iloc[j]
          #             else:
          #               better_length = abs(dfWindow.iloc[j]['rssi_length'] - mac_length) < abs(best_in_window['rssi_length'] - mac_length)
          #               better_stdev = abs(dfWindow.iloc[j]['rssi_stdev'] - mac_stdev) < abs(best_in_window['rssi_stdev'] - mac_stdev)
          #               better_mean = abs(dfWindow.iloc[j]['rssi_mean'] - mac_mean) < abs(best_in_window['rssi_mean'] - mac_mean)
          #               better_frequncy = abs(dfWindow.iloc[j]['frequency'] - df.iloc[i]['frequency']) < abs(best_in_window['frequency'] - df.iloc[i]['frequency'])
          #               if ( better_length | better_stdev | better_mean | better_frequncy):
          #                 best_in_window = dfWindow.iloc[j]