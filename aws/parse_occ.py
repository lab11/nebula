# Combines queue occ. csvs for different mules

import pandas
import numpy as np

N_MULES = 100

output = {'mule_id': [], 'time': [], 'occupancy': []}

for mule_id in range(N_MULES):
    # load csv and split columns
    data = pandas.read_csv("queue-occ/{}.csv".format(mule_id), header=None)
    times = data[0].tolist()
    occs = data[1].tolist()

    for t, o in zip(times, occs):
        output['mule_id'].append(mule_id)
        output['time'].append(int(t))
        output['occupancy'].append(o)
                
    df = pandas.DataFrame(output)
    df.to_csv('../express/occupancy_{}_mules_100k_rows_30_min.csv'.format(N_MULES), index=False)

