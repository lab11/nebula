import seaborn as sns 
import pandas as pd 
import numpy as np
import matplotlib.pyplot as plt

#set seaborn theme
sns.set_theme()
#sns.set_style("whitegrid")
sns.set_context("notebook", font_scale=2.1, rc={"lines.linewidth": 1.5}) 
sns.set(rc = {'figure.figsize':(10,7)})
sns.set(font_scale=1.4)
sns.set_style("whitegrid")
#plt.figure(figsize=(20, 7))

#import data and sanity check print
plaintext_df = pd.read_csv('latency.csv',names=["num_mules","latency"])
#print(plaintext_df.head(10))
#express_df = pd.read_csv('express-stats.csv',names=["num_mules","latency","throughput"])
express_df_1 = pd.read_csv('express_row_stats/ramp_1000000_rows.csv',names=["num_mules","latency","throughput"])
express_df_2 = pd.read_csv('express_row_stats/ramp_100000_rows.csv',names=["num_mules","latency","throughput"]) 
express_df_3 = pd.read_csv('express_row_stats/ramp_10000_rows.csv',names=["num_mules","latency","throughput"])
express_df_4 = pd.read_csv('express_row_stats/ramp_1000_rows.csv',names=["num_mules","latency","throughput"])
express_df_5 = pd.read_csv('express_row_stats/ramp_500000_rows.csv',names=["num_mules","latency","throughput"]) 
express_df_6 = pd.read_csv('express_row_stats/ramp_50000_rows.csv',names=["num_mules","latency","throughput"])

#import energy data 
energy_df = pd.read_csv('power_forplotting.csv') #,names=["plaintext","express_1","express_2","express_3","express_4","Time"])
print(energy_df.head(4))
energy_dfm = energy_df.melt("Time",var_name='cols',value_name='energy')
print(energy_dfm.head(4))

#plot latency
plt.figure(1)
sns.lineplot(x=plaintext_df.num_mules,y=plaintext_df.latency,label="Plaintext")
sns.lineplot(x=express_df_2.num_mules,y=express_df_2.latency,label="Express 100,000 Rows")
#sns.lineplot(x=express_df_3.num_mules,y=express_df_3.latency,label="Express 10000 Rows")
#sns.lineplot(x=express_df_4.num_mules,y=express_df_4.latency,label="Express 1000 Rows")
sns.lineplot(x=express_df_5.num_mules,y=express_df_5.latency,label="Express 500,000 Rows")
sns.lineplot(x=express_df_1.num_mules,y=express_df_1.latency,label="Express 1,000,000 Rows")
#sns.lineplot(x=express_df_6.num_mules,y=express_df_6.latency,label="Express 50000 Rows")
#plt.ylim(1,3)
plt.title("Latency at Scale")
plt.xlabel("Number of Mules Sending")
plt.ylabel("Latency (s)")
plt.savefig('latency.pdf',dpi=300) 

#plot throughput 
plt.figure(2)
sns.lineplot(x=plaintext_df.num_mules,y=(plaintext_df.num_mules/plaintext_df.latency),label="Plaintext")
sns.lineplot(x=express_df_2.num_mules,y=express_df_2.throughput,label="Express 100,000 Rows")
#sns.lineplot(x=express_df_3.num_mules,y=express_df_3.throughput,label="Express 10000 Rows")
#sns.lineplot(x=express_df_4.num_mules,y=express_df_4.throughput,label="Express 1000 Rows")
sns.lineplot(x=express_df_5.num_mules,y=express_df_5.throughput,label="Express 500,000 Rows")
sns.lineplot(x=express_df_1.num_mules,y=express_df_1.throughput,label="Express 1,000,000 Rows")
#sns.lineplot(x=express_df_6.num_mules,y=express_df_5.throughput,label="Express 50000 Rows")
plt.title("Throughput at Scale")
plt.xlabel("Number of Mules Sending")
plt.ylabel("Throughput (messages/sec)")
plt.savefig('throughput.pdf',dpi=300) 

plt.figure(3)
energy_plot = sns.lineplot(x="Time",y="energy",hue="cols",data=energy_dfm) #,kind="point")
handles, labels = energy_plot.get_legend_handles_labels()
energy_plot.legend(handles=handles[0:], labels=labels[0:])
plt.ylim=(2.5,5.0)
plt.xlabel("Time (s)")
plt.ylabel("Power (W)")
plt.title("Power Traces")
plt.savefig('energy.pdf',dpi=300)
print("saved figure")

energy_df = pd.read_csv('power_forplotting_2.csv',names=["plaintext","express_1","express_2","express_3","express_4","Time"])
average_a = np.mean(energy_df.plaintext)
average_b = np.mean(energy_df.express_1)
average_c = np.mean(energy_df.express_2)
average_d = np.mean(energy_df.express_3)
average_e = np.mean(energy_df.express_4)
print(average_a)
print(average_b)
print(average_c)
print(average_d)

power_avg = [average_b,average_c,average_d,average_e]
plaintext_power = [average_a,average_a,average_a,average_a]
rows = ['100k','200k','300k','400k']

plt.figure(4)
sns.lineplot(x=rows,y=power_avg, label="Express Power")
sns.lineplot(x=rows,y=plaintext_power, label="Plaintext Power")
plt.title("Average Power")
plt.xlabel("Express Rows")
plt.ylabel("Average Power (W)")
plt.savefig('average_power.pdf',dpi=300)

#plt.figure(figsize=(10, 7))
#plt.rc('xtick', labelsize=20)
#plt.rc('ytick', labelsize=20)
#plt.rc('legend', fontsize=18)
#plt.rc('axes', labelsize=20)  
#plt.rc('figure', titlesize=20)