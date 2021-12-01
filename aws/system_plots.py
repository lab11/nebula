import seaborn as sns 
import pandas as pd 
import matplotlib.pyplot as plt

#set seaborn theme
sns.set_theme()

#import data and sanity check print
plaintext_df = pd.read_csv('latency.csv',names=["num_mules","latency"])
print(plaintext_df.head(10))

#plot latency
plt.figure(1)
sns.lineplot(x=plaintext_df.num_mules,y=plaintext_df.latency)
plt.ylim(1,3)
plt.title("Latency vs. Parallel Messages")
plt.xlabel("Number of Mules Sending in Parallel")
plt.ylabel("Latency (s)")
plt.savefig('latency.jpg') #change to svg to generate higher fidelity plots

#plot throughput 
plt.figure(2)
sns.lineplot(x=plaintext_df.num_mules,y=(plaintext_df.num_mules/plaintext_df.latency))
plt.title("Throughput vs. Parallel Messages")
plt.xlabel("Number of Mules Sending in Parallel")
plt.ylabel("Throughput (msgs/sec)")
plt.savefig('throughput.jpg') #change to svg to generate higher fidelity plots