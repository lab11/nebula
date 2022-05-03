import pandas as pd
import json
import ast
import os
import streamlit as st
from stqdm import stqdm

st.header("Welcome to the Galaxy Dashboard")

def parse_json(file):
    df = pd.read_json(file, lines=True)
    df = df.replace(r'\n',' ', regex=True) # replace newline with space
    return df

@st.cache(suppress_st_warning=True)
def read_files(path):
    df = None
    files = os.listdir(path)
    for _file in stqdm(files):
        _file = path + _file
        _df = parse_json(_file)
        if df is None:
            df = _df
        else:
            df = df.append(_df)
    return df

def get_basic_stats(df):
    # Start and Stop time
    st.write(f"Data sniffing Start time: {df['time'].min()}")
    st.write(f"Data sniffing Stop time: {df['time'].max()}")
    # Name of the Gateway
    st.write(f"{len(df['hostname'].unique())} unique gateway(s): {df['hostname'].unique()}")
    # Number of devices scanned
    st.info(f"{len(df['msg'].unique())} unique device(s) scanned")


if __name__ == '__main__':
    passkey = st.text_input('Saisissez votre mot de passe')
    if passkey != "galaxy":
        st.write("Thanks for your interest. Enter the password to see the data")
    else:
        path = '/home/ubuntu/galaxy/deployments/data/' 
        st.write(f"Number of files: {len(os.listdir(path))}")
        df = read_files(path)
        get_basic_stats(df)

        if st.checkbox("Show Raw Data"):
            st.dataframe(df)

        if st.checkbox('Filter Data'):
            select_gateway = sorted(df['hostname'].unique())
            select_gateway_dropdown = st.sidebar.multiselect('Select 1/more gateways(s):', select_gateway)
            tmax = df['time'].max()
            tmin = df['time'].min()
            select_time_range = sorted(df['time'].unique())
            select_time_slider = st.sidebar.select_slider('Time range:', options=select_time_range, value=(tmax, tmin))
            start_time, end_time = select_time_slider[0], select_time_slider[1]
            df_gateway_time_slice = df.where((df.hostname.isin(select_gateway_dropdown)) & (df.time >= start_time) & (df.time <= end_time))
            df_gateway_time_slice = df_gateway_time_slice.dropna()
            st.dataframe(df_gateway_time_slice)
