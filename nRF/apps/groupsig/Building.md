# How to make, and build the groupsig library
## Includes all dependencies for Ubuntu 18.04LTS

sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 6AF7F09730B3F0A4
sudo apt update
sudo apt-get install libmpfr-dev
git clone https://github.com/openssl/openssl.git
cd openssl/
./Configure
make
make test
sudo make install
cd ../
git clone https://github.com/IBM/libgroupsig.git
cd libgroupsig/
cd build/
cmake ..
make 
sudo make install

