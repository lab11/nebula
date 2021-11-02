# Attack Simulations

The purpose of this directory is to explore ways in which a naive data backhaul system can be used to track mule and sensor movement through upload metadata. For now, we consider two attacks:

1. Assume we know the locations of all the sensors. Given a sequence of sensor IDs and transmission times uploaded by a mule, a third party network service provider can infer the paths of the mules, and subsequently deanonymize mules based on their activity patterns.

2. Assume we only know where a few of the sensors are. Given sequences of sensor IDs and transmission times uploaded by many mules, a third-party network service provider can infer the locations of all other sensors, and subsequently carry out attack 1 with less prior information.

To attempt these attacks, we explore the [Geolife database](https://www.microsoft.com/en-us/download/details.aspx?id=52367) and use the recorded trajectories as movement data for simulated mules. We then simulate data backhaul by placing 1 million BLE sensors around the streets of Beijing and replay the trajectories recorded in Geolife. We collect sequences of sensor ID and upload time for each mule as well as the ground truth position of each sensor and trajectory of each mule and uploaded the data in a [Google Drive folder](https://drive.google.com/drive/folders/1Xn1ERyo0_UMMv4xOimQheJ8HRrrYh2Zn?usp=sharing). The code used to generate such data is included in the `exploring_geolife.ipynb` Jupyter Notebook.
