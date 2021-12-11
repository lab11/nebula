import pyvisa 
import time
import csv

rm = pyvisa.ResourceManager()
resources = rm.list_resources()

#assume only one device connected
chroma = resources[0]
print(chroma)

#open device communication 
inst = rm.open_resource(chroma)
inst.timeout = 20000

#save a measurements array
measurement_list = []

#take measurements every .2 seconds 
num_measurements = 300
count = 0
while (count < num_measurements):
    data = inst.query("MEASure?")
    #print(data)
    data_list = data.split(",")
    #print(data_list)
    power = data_list[10]
    print("power:", power)
    measurement_list.append(power)
    count +=1
    time.sleep(0.2)

#export to csv
print(measurement_list)
with open('power_plaintext.csv', mode='a') as csvfile:
    csvwriter = csv.writer(csvfile,delimiter=',')
    csvwriter.writerow(["plaintext"])
    csvwriter.writerow(measurement_list)
 


