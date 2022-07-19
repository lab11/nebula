/* Bluetooth Advertisement Sniffer (simple)*/
/* Run: sudo node sniffer.js */

const bunyan = require('bunyan'); // Logging to file
const crypto = require('node:crypto'); // Hash MACs
const fs = require('fs');
const noble = require('@abandonware/noble')  // BLE Scanner Library

var service  = {
	// listen for all services
}
var services = Object.values(service)
var timeout  = null

/* Logger */

var date_ob = new Date();

var log_path = '/var/log/sniffer_'+date_ob.getMonth()+'-'+date_ob.getDate()+'-22_'+date_ob.getHours()+':'+date_ob.getMinutes()+':'+date_ob.getSeconds()+'.log';
console.log('output -> ' + log_path);

var log = fs.createWriteStream(log_path, {flags: 'a'});
log.write('ts,mac_hash,rssi\n');

/* Bluetooth Scanner */

noble.on('stateChange', state => {
    console.log('Bluetooth Scanner is: ' +  state)
    if (state === 'poweredOn') {
        noble.startScanning(services, true);
    } else {
	console.log('Bluetooth scanner stopped');
        noble.stopScanning()
    }
})

noble.on('discover', peripheral => {
    // filter out known OUIs
    if (peripheral['addressType'] === 'random') {		
        const hash = crypto.createHash('sha256');
        hash.update(peripheral['address']);
        let mac_hash = hash.digest('hex');

        log.write(new Date().getTime()+','+mac_hash+','+peripheral['rssi']+'\n');
    }
})


