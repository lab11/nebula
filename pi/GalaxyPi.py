import struct
from bluepy.btle import ScanEntry, Peripheral, Scanner, DefaultDelegate
import argparse

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

# start the scanner for 5 seconds 
scanner.start(5)
scanner.process()
scanner.stop()
#while True: //scan 10 times
#count = 0
#while count < 10:
#    print("Still running...")
#    scanner.process()
#    count += 1
#    print(count)

#Get service and connect

GALAXY_SERVICE_UUID = "32e61089-2b22-4db5-a914-43ce41986c70"
GALAXY_CHAR_UUID    = "32e6108a-2b22-4db5-a914-43ce41986c70"

try:
    print("connecting")
    nRF = Peripheral(addr)

    print("connected")

    # Get service
    sv = nRF.getServiceByUUID(GALAXY_SERVICE_UUID)
    # Get characteristic
    ch = sv.getCharacteristics(GALAXY_CHAR_UUID)[0]

    while True:
        print(ch.read())
        input("Press any key to get sensor data")
        led_state = bool(int(ch.read().hex()))
        #ch.write(bytes([not led_state]))
finally:
    nRF.disconnect()

