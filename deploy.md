Instructions for end users:
TBD

#Instructions if you own the private key:
```
cd ota/src
```
#initial steps to be expanded

mkdir ../certificates/0.0.5v
cp ../certificates/certs.sector ../certificates/0.0.5v
#set local.mk to the ota-main program
make -j4 rebuild OTAVERSION=0.0.5
mv firmware/otamain.bin ../certificates/0.0.5v
#set local.mk back to ota-boot program
make -j4 rebuild OTAVERSION=0.0.5
cp firmware/otaboot.bin ../certificates/0.0.5v
#commit this as version 0.0.5
#set up a new github release 0.0.5 as a pre-release using the just commited master...

#erase the flash and uplaod the privatekey
```
esptool.py -p /dev/cu.usbserial-* --baud 230400 erase_flash 
esptool.py -p /dev/cu.usbserial-* --baud 230400 write_flash 0xf5000 privatekey.der
```
#upload the ota-boot program to the device that contains the private key
make flash
#power cycle to prevent the bug for software reset after flash
#create the 3 signature files next to the bin file and upload to github one by one
#verify the hashes on the computer
openssl sha384 ../certificates/otamain.bin
xxd ../certificates/otamain.bin.sig
#make the release a production release on github
#remove the private key
```
esptool.py -p /dev/cu.usbserial-* --baud 230400 write_flash 0xf5000 ../certificates/blank.bin
```
