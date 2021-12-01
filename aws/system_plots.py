import seaborn as sns 
import pandas as pd 
import matplotlib.pyplot as plt

#set seaborn theme
sns.set_theme()

#import data and sanity check print
plaintext_df = pd.read_csv('latency.csv',names=["num_mules","latency"])
print(plaintext_df.head(10))
express_df = pd.read_csv('express-stats.csv',names=["num_mules","latency","throughput"])

#plot latency
plt.figure(1)
plt.figure(figsize=(10, 7))
plt.rc('xtick', labelsize=20)
plt.rc('ytick', labelsize=20)
plt.rc('legend', fontsize=18)
plt.rc('axes', labelsize=20)  
plt.rc('figure', titlesize=20)
sns.lineplot(x=plaintext_df.num_mules,y=plaintext_df.latency,label="plaintext")
sns.lineplot(x=express_df.num_mules,y=express_df.latency,label="express")
#plt.ylim(1,3)
plt.title("Latency at Scale")
plt.xlabel("Number of Mules Sending")
plt.ylabel("Latency (s)")
plt.savefig('latency.pdf',dpi=300) #change to svg to generate higher fidelity plots

#plot throughput 
plt.figure(2)
plt.figure(figsize=(10, 7))
plt.rc('xtick', labelsize=20)
plt.rc('ytick', labelsize=20)
plt.rc('legend', fontsize=18)
plt.rc('axes', labelsize=20)  
plt.rc('figure', titlesize=20)
sns.lineplot(x=plaintext_df.num_mules,y=(plaintext_df.num_mules/plaintext_df.latency),label="plaintext")
sns.lineplot(x=express_df.num_mules,y=express_df.throughput,label="express")
plt.title("Throughput at Scale")
plt.xlabel("Number of Mules Sending")
plt.ylabel("Throughput (messages/sec)")
plt.savefig('throughput.pdf',dpi=300) #change to svg to generate higher fidelity plots