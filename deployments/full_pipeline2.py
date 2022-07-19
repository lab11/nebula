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

import glob
import argparse

import pickle5 as pickle
from matplotlib import pyplot as plt

# Puts all the time and rssi values for each unique mac_hash into lists.
def get_log_df(data_path):
    # Load in our data
    dfBoi = pd.read_csv(data_path)
    
    # It looks like there's a jump in time from before time is synched,
    # so let's just delete the first few observations with bad times
    min_time = 1657100000000
    dfBoi = dfBoi.loc[dfBoi['ts'] > min_time]
    
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

    # Crawl through the pickles and generates figures for each pickle.
    for aPickle in glob.glob('{}/*.pkl'.format(args.pkls_dir)):
        # Read in our pickle.
        dfBoi = read_pickle_to_df(aPickle)
        location = aPickle.split('/')[-1].split('.')[0]
        fig_file = "{}/{}.png".format(args.figs_dir, location)
        if os.path.exists(fig_file):
            continue
        
        # Generate our figure :D
        print("Generating figure for {}.".format(location))
        min_time, max_time = calculate_interactions(dfBoi, maxgap=args.max_gap, mincon=args.min_con)
        
        # Set up the figure to look nice.
        fig = plt.figure(figsize=(20,15))
        grid = plt.GridSpec(4, 1, hspace=0.3)
        top_ax = fig.add_subplot(grid[:3, 0])
        bottom_ax = fig.add_subplot(grid[3, 0])
        
        # Set up things to draw individual interactions over time.
        colors = ['b', 'g', 'r', 'c', 'm', 'y']
        numColors = len(colors)
        top_ax.set_title('Interaction intervals for various seen devices at {} (maxgap = {}s, mincon = {}s)'.format(location, args.max_gap, args.min_con))
        top_ax.set_xlabel('Time (sec)')

        # Set up things to count the number of concurrent interactions over time.
        num_bins = 10000
        period = (max_time - min_time) / num_bins
        interaction_count = np.zeros(num_bins + 1)
        bottom_ax.set_title('Number of concurrent interactions at {} over time (maxgap = {}s, mincon = {}s)'.format(location, args.max_gap, args.min_con))
        bottom_ax.set_xlabel('Time (sec)')
        bottom_ax.set_ylabel('Number of concurrent interactions')
        bottom_ax.set_yscale('symlog')
        bottom_ax.grid(which='both', alpha=0.3)

        # Iterate through all the MACs.
        for i in tqdm(range(len(dfBoi))):

            macID = dfBoi.iloc[i]['mac_hash']
            interaction_list = dfBoi.iloc[i]['interactions']
            
            # Plot and record the interactions from useful MACs.
            for interaction in interaction_list:
                top_ax.plot(interaction, [i, i], 'x-', color=colors[i % numColors])
                interaction_count[int((interaction[0]-min_time)/period):int((interaction[1]-min_time)/period)] += 1

        # Plot the count of concurrent interaction times.
        bottom_ax.plot(np.linspace(min_time, max_time, num_bins+1), interaction_count)

        # Save our figure.
        plt.savefig(fig_file)
        plt.close()


