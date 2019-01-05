Instructions for end users:
TBD

#Instructions if you own the private key:
```
cd ota/src
```
#initial steps to be expanded

mkdir ../certificates/0.3.0v
cp ../certificates/certs.sector* ../certificates/0.3.0v
#set local.mk to the ota-main program
make -j6 rebuild OTAVERSION=0.3.0
mv firmware/otamain.bin ../certificates/0.3.0v
#set local.mk back to ota-boot program
make -j6 rebuild OTAVERSION=0.3.0
cp firmware/otaboot.bin ../certificates/0.3.0v
#commit this as version 0.3.0
#set up a new github release 0.3.0 as a pre-release using the just commited master...

#erase the flash and upload the ota-boot program to the device that contains the private key
```
esptool.py -p /dev/cu.usbserial-* --baud 230400 erase_flash 
make flash OTAVERSION=0.3.0
```
#run the code to change the sysparam area already
#upload the privatekey
```
esptool.py -p /dev/cu.usbserial-* --baud 230400 write_flash 0xfa000 privatekey.der
```
#power cycle to prevent the bug for software reset after flash
#create the 3 signature files next to the bin file and upload to github one by one
#verify the hashes on the computer
openssl sha384 ../certificates/otamain.bin
xxd ../certificates/otamain.bin.sig
#make the release a production release on github
#remove the private key
```
esptool.py -p /dev/cu.usbserial-* --baud 230400 write_flash 0xfa000 ../certificates/blank.bin
```
