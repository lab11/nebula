# A Python script to run the attack simulations faster.
# Written by Alvin Tan.
# On 11/02/2021.
# At UC Berkeley.

# For more details about the code in this script, see `exploring_geolife.ipynb`.


# Import libraries.
import os 
import gc 
import glob 
import math 
import pickle 
import numpy as np 
import pandas as pd 
from scipy.interpolate import interp1d 


''' Avoid loading all the mules' data at once, since it's very large (~1.4 GB)
dict_path = "ground_truth/big_dict.pkl"
with open(dict_path, 'rb') as dict_file:
    big_dict = pickle.load(dict_file)
'''

# Load sensor data from file.
sensor_path = "ground_truth/sensors.csv"
with open(sensor_path, 'r') as sensor_file:
    sensor_data = np.genfromtxt(sensor_file, delimiter=',', skip_header=1, usecols=(1,2,3))
num_sensors = sensor_data.shape[0]
sensor_points = sensor_data[:, :2].reshape((num_sensors, 1, 2))
sensor_radii = sensor_data[:, 2].reshape((num_sensors, 1, 1))


# Generate simulated trace information for the attacks.
mules_dir = r"ground_truth/mules/*.csv"
for mule_path in glob.iglob(mules_dir):
    mule_name = mule_path.split('/')[-1]
    
    # Skip this person if there is already simulated data for them.
    save_path = "sim_data/mules/{}".format(mule_name)
    if os.path.exists(save_path):
        print("Skipping {} because it has already been processed.".format(mule_name[:3]))
        continue 
    
    # Load mule data from file.
    mule_data = np.loadtxt(mule_path, delimiter=',', skiprows=1)
    s = mule_data[:,2]
    xyt = mule_data[:,[0,1,3]]
    f = interp1d(s, xyt, axis=0)
    
    # Send broadcasts every 0.5 seconds.
    num_samples = int(s[-1] / 0.5)
    s_int = np.linspace(s[0], s[-1], num=num_samples, endpoint=True)
    xyt_int = f(s_int)

    # Split the calculation into smaller chunks so calculation fits in memory.
    steps_per_split = 1000
    num_splits = math.ceil(num_samples / steps_per_split)

    # Print statement for a sanity check.
    print("Starting processing for {}, which has {} time steps.".format(mule_name[:3], num_samples))

    # Save our results to file.
    with open(save_path, 'w') as save_file:
        save_file.write('sensor ID,norm_secs (secs),time (days)\n')

        # Calculate the distance from each node at each time step.
        cur_steps = 0
        for i in range(num_splits): 
            #dist = np.linalg.norm(sensor_points - pos_int[i*steps_per_split:(i+1)*steps_per_split, :], axis=2)
            #sensor_ids = np.argwhere((dist < sensor_radii).T) # Time step, sensor ID.
            #seen_sensors = np.vstack([sensor_ids[:,1], s_int[sensor_ids[:, 0] + i*steps_per_split], t_int[sensor_ids[:, 0] + i*steps_per_split]]).T # Sensor ID, norm_secs, time.
            
            # Calculate interactions based on infinity norm instead of Euclidean norm to simplify calculations.
            mask = (np.abs(sensor_points - xyt_int[cur_steps:cur_steps + steps_per_split, :1]) < sensor_radii).all(axis=2)
            sensor_ids = np.argwhere(mask.T)
            seen_sensors = np.vstack([sensor_ids[:, 1], s_int[sensor_ids[:, 0] + cur_steps], xyt_int[sensor_ids[:, 0] + cur_steps, 2]]).T # Sensor ID, norm_secs, time.
            np.savetxt(save_file, seen_sensors, delimiter=',')
            cur_steps += steps_per_split

            # Explicitly delete matrices and run the garbage collector to avoid running out of memory.
            del mask, sensor_ids, seen_sensors 
            gc.collect()

    # Explicitly delete variables and run the garbage collector to avoid running out of memory.
    del mule_data, s, xyt, f, s_int, xyt_int
    gc.collect()



