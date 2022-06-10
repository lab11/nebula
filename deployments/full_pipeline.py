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
import swifter

import glob
import argparse
from OuiLookup import OuiLookup
import macaddress

# Puts all the time and rssi values for each unique MAC into lists.
def get_aggregates(df):
    ndf = df[['mac', 'time', 'rssi']]
    ndf = ndf.sort_values(['time'])
    ndf = ndf.groupby('mac', as_index=False)['time', 'rssi'].agg(list)
    ndf['reps'] = ndf['rssi'].str.len()
    # df['startTime'] = df["time"].str[0]
    # df['endTime'] = df["time"].str[-1]
    return ndf

# Filters out all the junk from the messages to get the fields we are actually interested in.
def analyze_data(df):
    df['time'] = df['time'].apply(lambda x: parser.parse(x).timestamp())
    df['time'] = df['time'].swifter.apply(lambda x : pd.to_numeric(x))
    df['mac'] = df.msg.str.split(r"address", expand=True)[1].str[3:20]
    df['rssi'] = df.msg.str.split(r"rssi", expand=True)[1].str[2:5]
    df['rssi'] = df['rssi'].swifter.apply(lambda x : pd.to_numeric(x))
    df['addressType'] = df.msg.str.split(r"addressType", expand=True)[1].str[3:9]
    dfagg = get_aggregates(df)
    return dfagg

# Clean up the raw logs so it can actually be read as JSON.
def parse_json(file):
    df = pd.read_json(file, lines=True)
    # The msg indices are hardcoded downstream. So, any changes here might break.
    df["msg"] = df["msg"].replace(r'discovered peripheral Peripheral',' ', regex=True) # replace useless text
    df["msg"] = df["msg"].replace(r'\n','"', regex=True) # replace newline with space
    df["msg"] = df["msg"].replace(r': ','":', regex=True) # make it a dict
    # df["msg"] = df["msg"].replace(r' ','', regex=True) # get rid of space
    return df

# Iterate through all the raw logs in a directory 
def read_files(path):
    df = None
    files = os.listdir(path)
    print(f"Reading {len(files)} Data Files")
    for _file in tqdm(files):
        _file = path + _file
        _df = parse_json(_file)
        if df is None:
            df = _df
        else:
            df = df.append(_df)
    return df

# Generates a dictionary of OUI descriptions from a file of known OUIs.
def generate_oui_descriptions(oui_path):
    with open(oui_path, 'r') as f:
        oui_dataset_lines = f.readlines()
    
    oui_descriptions = {}
    for l in oui_dataset_lines:
        if not l or l[0] == '\n' or l[0] == '#':
            continue

        description = l.split('\t', 1)[1].strip()

        # unfortunately, some of the OUIs are the expected 3 octets, some are more specific prefixes...
        raw_oui = l.split('\t', 1)[0].strip()

        try:
            oui = macaddress.OUI(raw_oui)
            oui_descriptions[oui] = {'description': description}
        except ValueError:
            raw_oui_parts = raw_oui.split('/')
            mac = macaddress.MAC(raw_oui_parts[0])
            oui_descriptions[mac] = {'prefix_bits': int(raw_oui_parts[1]), 'description': description}
    return oui_descriptions

# Return the OUI in oui_descriptions that is closest to mac_string.
# Returns None if none of them are close.
def find_oui_match(oui_descriptions, mac_string):
    mac = macaddress.MAC(mac_string)
    if mac.oui in oui_descriptions:
        return mac.oui
    return None

# Read a pickle to a dataframe.
def read_pickle_to_df(aPickle):
    with open(aPickle, 'rb') as fh:
        df = pickle.load(fh)
        return df

# Identify all the interactions and append them to a list.
def calculate_interactions(df, maxgap=10.1, mincon=2.5, col='time'):

  all_intervals = []
  min_time = np.inf
  max_time = -np.inf

  for i in tqdm(range(len(df))):
    macID = df.iloc[i]['mac']

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
    parser = argparse.ArgumentParser(description='Generates interaction figures from raw log files collected by a Noble BLE sniffer.')
    ## For reading and processing log files.
    parser.add_argument('--logs_dir', type=str, default='./data', help='Path to a directory of directories of log files.')
    parser.add_argument('--pkls_dir', type=str, default='./pkls', help='Path to a directory where we store processed log files as pickles.')
    # For generating interactions with seen BLE devices.
    parser.add_argument('--oui_list', type=str, default='./wireshark_oui_list.txt', help='Path to a file with known OUI prefixes.')
    parser.add_argument('--max_gap', type=float, default=10.1, help='Maximum time (seconds) between advertisements before we consider it another interaction.')
    parser.add_argument('--min_con', type=float, default=2.5, help='Minimum considered connection duration (seconds).')
    parser.add_argument('--figs_dir', type=str, default='./figs', help='Path to a directory where we store the generated figures.')

    args = parser.parse_args()

    # Crawl through a directory of directories of log files and process them all into pickles.
    for aDir in glob.glob('{}/*'.format(args.logs_dir)):
        location_name = aDir.split('/')[-1]
        df = read_files(aDir)
        print(df.describe())
        print("Starting feature extraction..")
        df = analyze_data(df)
        pkl_file = "{}/{}.pkl".format(args.pkls_dir, location_name)
        print("Writing pickled logs to {}.".format(pkl_file))
        df.to_pickle(pkl_file)
    
    # Generate list of known OUIs to filter out.
    oui_descriptions = generate_oui_descriptions(args.oui_list)
    
    # Crawl through the pickles and generates figures for each pickle.
    for aPickle in glob.glob('{}/*.pkl'.format(args.pkls_dir)):
        # Read in our pickle.
        dfBoi = read_pickle_to_df(aPickle)
        location = aPickle.split('/')[-1].split('.')[0]
        
        # Generate our figure :D
        print("Generating figure for {}.".format(location))
        min_time, max_time = calculate_interactions(dfBoi, maxgap=args.max_gap, mincon=args.min_con)
        
        # Set up the figure to look nice.
        fig = plt.figure(figsize=(20,15))
        grid = plt.GridSpec(4, 1, hspace=0.3)
        top_ax = fig.add_subplot(grid[:3, 0])
        bottom_ax = fig.add_subplot(grid[3, 0])
        
        # Set up things to draw individual interactions over time.
        plotted = 0
        colors = ['b', 'g', 'r', 'c', 'm', 'y']
        numColors = len(colors)
        top_ax.set_title('Interaction intervals for various MAC addresses at {} (maxgap = {}s, mincon = {}s)'.format(location, args.max_gap, args.min_con))
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

            macID = dfBoi.iloc[i]['mac']
            interaction_list = dfBoi.iloc[i]['interactions']
            
            # Plot and record the interactions from useful MACs.
            if not find_oui_match(oui_descriptions, macID) and len(interaction_list) > 0:
                for interaction in interaction_list:
                    top_ax.plot(interaction, [plotted, plotted], 'x-', color=colors[plotted % numColors])
                    interaction_count[int((interaction[0]-min_time)/period):int((interaction[1]-min_time)/period)] += 1
                plotted += 1

        # Plot the count of concurrent interaction times.
        bottom_ax.plot(np.linspace(min_time, max_time, num_bins+1), interaction_count)

        # Save our figure.
        plt.savefig("{}/{}.png".format(args.figs_dir, location))


