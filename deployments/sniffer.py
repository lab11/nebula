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

def get_aggregates(df):
    ndf = df[['mac', 'time', 'rssi']]
    ndf = ndf.groupby('mac', as_index=False)['time', 'rssi'].agg(list)
    ndf['reps'] = ndf['rssi'].str.len()
    # df['startTime'] = df["time"].str[0]
    # df['endTime'] = df["time"].str[-1]
    return ndf

def analyze_data(df):
    df['time'] = df['time'].apply(lambda x: parser.parse(x).timestamp())
    df['time'] = df['time'].swifter.apply(lambda x : pd.to_numeric(x))
    df['mac'] = df.msg.str.split(r"address", expand=True)[1].str[3:20]
    df['rssi'] = df.msg.str.split(r"rssi", expand=True)[1].str[2:5]
    df['rssi'] = df['rssi'].swifter.apply(lambda x : pd.to_numeric(x))
    df['addressType'] = df.msg.str.split(r"addressType", expand=True)[1].str[3:9]
    dfagg = get_aggregates(df)
    return dfagg


def parse_json(file):
    df = pd.read_json(file, lines=True)
    # The msg indices are hardcoded downstream. So, any changes here might break.
    df["msg"] = df["msg"].replace(r'discovered peripheral Peripheral',' ', regex=True) # replace useless text
    df["msg"] = df["msg"].replace(r'\n','"', regex=True) # replace newline with space
    df["msg"] = df["msg"].replace(r': ','":', regex=True) # make it a dict
    # df["msg"] = df["msg"].replace(r' ','', regex=True) # get rid of space
    return df

def read_files(path):
    df = None
    files = os.listdir(path)
    print(f"Reading {len(files)} Data Files")
    for _file in tqdm(files[:1]):
        _file = path + _file
        _df = parse_json(_file)
        if df is None:
            df = _df
        else:
            df = df.append(_df)
    return df


if __name__ == '__main__':
    path = '/home/ubuntu/galaxy/deployments/data/logs-540ab/' 
    df = read_files(path)
    print(df.describe())

    print("Starting feature extraction..")
    df = analyze_data(df)
    breakpoint()
    # df.to_pickle("test.pkl")



