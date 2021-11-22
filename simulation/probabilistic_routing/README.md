# Simulation Data using Probabilistic Routing

Instead of using raw mobility traces in our simulations, we want to have some concept of expected traffic flow and instantiate an arbitrary number of mules and sensors that interact accordingly. Thus, we construct a heatmap and generate trajectories using randomized starting and ending positions with a randomly generated trajectory in between the two. Each step in the trajectory is weighted by values in the traffic heatmap and biased to continue in the same direction while making progress towards the target destination.

The creation of the heatmap, generation of weighted random walks, and collection of simulation data is done in `probabilistic_trajectories.ipynb`. The last section of the notebook, which is titled "Collect simulation data", is stand-alone and can be modified and run to generate additional simulation results without running the entirety of the notebook.

## Simulation setup and data format

We ran the simulation with 100 mobile mules and 1000 stationary sensors over one hour of time. Each mule has its own speed uniformly randomly selected to be between 1 and 15 meters per second, and each sensor has a range uniformly randomly selected to be between 10 and 20 meters. We ignore WiFi hotspots for now and assume that the mule is able to upload from anywhere using a cellular data plan. Simulation results are stored in three different files, described below.

1. `prob_data/interactions.csv` holds the results of the simulation. We want to collect interaction data and some concept of expected traffic flow. The collected data has the following format:

| sensor_id | mule_id | interaction_time | interaction_duration |
| :---: | :---: | :---: | :---: |
| i = 1, ..., 1000 | j = 1, ..., 100 | seconds | seconds |

where `interaction_time` is the time in seconds when the mule walks within range of the sensor and `interaction_duration` is the amount of time the mule spends within the range of the sensor before leaving.

2. `prob_data/sensor_metadata.csv` contains some metadata on the sensors, which were uniformly randomly deployed across the available area. The collected data has the following format:

| sensor_id | x | y | radius | traffic |
| :---: | :---: | :---: | :---: | :---: |
| i = 1, ..., 1000 | x = \[0, 800\] | y = \[0, 950\] | r = \[10, 20\] | float |

where `x, y, r` is in meters and `traffic` is the value of the heatmap in the position that the sensor is at.

3. `prob_data/mule_metadata.csv` contains some metadata on the mules and has the following format:

| mule_id | speed | displacement |
| :---: | :---: | :---: |
| j = 1, ..., 100 | s = \[1, 15\] | meters | 

where `s` is in meters per second and `displacement` is the Euclidean distance between the mule's randomly selected start and target positions. The purpose of `displacement` is to provide some information about the mule's trajectory without needing to record the actual trajectory for each mule.

## Proposed utility of simulation results

Using these simulation results, we can calculate expected latency measures for each sensor and generate a realistic workload for a centralized clearing house. Depending on how long we expect each connection to take, we can filter out brief interactions from our workload. We can also subsample the sensors and mules to consider the effects of scaling, consider workloads experienced by each individual mule, and apply other interaction constraints based on features like memory usage restrictions and batched uploads.
