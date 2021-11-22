import struct
from bluepy.btle import ScanEntry, Peripheral, Scanner, DefaultDelegate
import argparse
import time
import csv
import sys

script_start = time.time()

parser = argparse.ArgumentParser(description='Print advertisement data from a BLE device')
parser.add_argument('addr', metavar='A', type=str, help='Address of the form XX:XX:XX:XX:XX:XX')
args = parser.parse_args()
addr = args.addr.lower()
if len(addr) != 17:
    raise ValueError("Invalid address supplied")

class ScanDelegate(DefaultDelegate):
    def __init__(self):
        DefaultDelegate.__init__(self)

    # when this python script discovers a BLE broadcast packet from a buckler
    # advertising light measurements, print out the data
    def handleDiscovery(self, dev, isNewDev, isNewData):
        if dev.addr == addr:
            print("Found advertisement from: ", str(dev.addr))
            print("Name: " + str(dev.getValueText(ScanEntry.COMPLETE_LOCAL_NAME)))
            data = dev.getValue(ScanEntry.MANUFACTURER)
            if data is not None:
                print("Data: " + str(data.hex()))

# create a scanner object that sends BLE broadcast packets to the ScanDelegate
scanner = Scanner().withDelegate(ScanDelegate())

# start the scanner for 0.5 second
#scanner.start(0.5)
#scanner.process()
#scanner.stop()
#while True: //scan 10 times
#count = 0
#while count < 10:
#    print("Still running...")
#    scanner.process()
#    count += 1
#    print(count)

# Set up CSV

# Get service and connect

time_after_ad = time.time()

GALAXY_SERVICE_UUID = "32e61089-2b22-4db5-a914-43ce41986c70"
GALAXY_CHAR_UUID    = "32e6108a-2b22-4db5-a914-43ce41986c70"

try:
    print("connecting")
    before_connecting_time = time.time()
    nRF = Peripheral(addr)
    after_connecting_time = time.time()
    print("connected")
    connection_time = after_connecting_time - before_connecting_time
    #print(connection_time)

    # Get service
    sv = nRF.getServiceByUUID(GALAXY_SERVICE_UUID)
    # Get characteristic
    ch = sv.getCharacteristics(GALAXY_CHAR_UUID)[0]

    # Save 1 data packets 
    count = 0
    all_data = []
    while (count < 1):
        value = ch.read()
        #print(ord(value[0]))
        data = ord(value[0])
        count+=1

        print("Data size ", sys.getsizeof(data))
        print("Total packet size ", sys.getsizeof(value))

        #TODO: Save data to data buffer 
        all_data += [[count,data]]

    after_getting_data_time = time.time()

    # setup and write CSV
    csv_headers = ['count','data']
    filename = "nRf_data.csv"

    #print(all_data)

    with open(filename,'w') as csvfile:
        csvwriter = csv.writer(csvfile)
        csvwriter.writerow(csv_headers)
        csvwriter.writerows(all_data)

finally:
    nRF.disconnect()
    script_end = time.time()
    print("Total time to get data from nRF to Pi csv over BLE ",script_end-script_start)
    #print("Advertising time ", time_after_ad-script_start)
    print("Connection to end ", script_end-time_after_ad)
    print("Conenction time ", connection_time)
    print("Data transfer time", after_getting_data_time-after_connecting_time)
    print(" Write to CSV time", script_end-after_getting_data_time)

