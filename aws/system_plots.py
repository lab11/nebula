import seaborn as sns 
import pandas as pd 
import matplotlib.pyplot as plt

#set seaborn theme
sns.set_theme()
sns.set_style("whitegrid")
sns.set_context("notebook", font_scale=1.05, rc={"lines.linewidth": 2.5})

#import data and sanity check print
plaintext_df = pd.read_csv('latency.csv',names=["num_mules","latency"])
print(plaintext_df.head(10))
#express_df = pd.read_csv('express-stats.csv',names=["num_mules","latency","throughput"])
express_df_1 = pd.read_csv('express_row_stats/ramp_1000000_rows.csv',names=["num_mules","latency","throughput"])
express_df_2 = pd.read_csv('express_row_stats/ramp_100000_rows.csv',names=["num_mules","latency","throughput"]) 
express_df_3 = pd.read_csv('express_row_stats/ramp_10000_rows.csv',names=["num_mules","latency","throughput"])
express_df_4 = pd.read_csv('express_row_stats/ramp_1000_rows.csv',names=["num_mules","latency","throughput"])
express_df_5 = pd.read_csv('express_row_stats/ramp_500000_rows.csv',names=["num_mules","latency","throughput"]) 
express_df_6 = pd.read_csv('express_row_stats/ramp_50000_rows.csv',names=["num_mules","latency","throughput"])


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

#plt.figure(figsize=(10, 7))
#plt.rc('xtick', labelsize=20)
#plt.rc('ytick', labelsize=20)
#plt.rc('legend', fontsize=18)
#plt.rc('axes', labelsize=20)  
#plt.rc('figure', titlesize=20)