# OTA
Universal OverTheAir firmware update system for esp-open-rtos based GitHub repositories

## Design
This is a repository in the making. It starts with a good design so I started with the flowchart.

![](https://github.com/HomeACcessoryKid/ota/blob/master/design-v1.png)

### Concepts
```
boot=slot1
baseURL=repo
version=x
```
This represents that in sector1 used by rboot, we will also store the following info
- baseURL: everything is intended to be relative to https://github.com, so this info is the user/repo part
- version: the version that this repo is currently running at

After this we run the OTA code which will try to deposit in boot slot 0 the latest version of the baseURL repo.

```
t
```
This represents an exponential hold-off to prevent excesive hammering on the github servers. It resets at a power-cycle.

```
download certificate signature
certificate update?
Download Certificates
```
This is a file that contains the checksum of the sector containing three certificates/keys
- public key of HomeACessoryKid that signs the certificate/key sector 
- root CA used by GitHub
- root CA used by the DistributedContentProvider (Amazon for now)

First, the file is intended to be downloaded with server certificate verification activated. If this fails, it is downloaded anyway without verification and server is marked as invalid. Once downloaded, the sha256 checksum of the active sector is compared to the checksum in the signature file. If equal, we move on. If not, we download the updated sector file to the standby sector.

```
signature match?
```
From the sector containing up to date certificates the sha256 hash is signed by the private key of HomeACessoryKid.
Using the available public key, the validity is verified

```
server valid?
```
If in the previous steps the server is marked invalid, we return to the main app in boot slot 0 and we report by syslog to a server (to be determinded) so we learn that github has changed its certificate CA provider and HomeACessoryKid can issue a new certificate sector.

```
new OTA version?
self-updater(0) update OTAapp➔1
checksum OK?
```
Now that the downloading from GitHub has been secured, we can trust whatever we download based on a checksum.
We verify if there is an update of this OTA repo itself? If so, we use a self-updater (part of this repo) to 'self update'. After this we have the latest OTA code.

```
OTA app(1) updates Main app➔0
checksum OK?
```
Using the baseURL info and the version as stored in sector1, the latest binary is found and downloaded if needed. If the checksum does not work out, we return to the OTA app start point considering we cannot run the old code anymore.
But normally we boot the new code and the mission is done.

Note that switching from boot=slot1 to boot=slot0 does not require a reflash

## Providing the images
The intention is that the image that needs to be flashed will be available in a GitHub offical release using Travis.
This implies two things:
- there is an actual version number to work with and an unique entry-point 'latest'
- Github and Travis take care that the process is 100% documented so full code review can take place at all times to keep everyone honest

For the moment have a look at a sample of this idea at [FOTA-test](https://github.com/HomeACcessoryKid/FOTAtest/releases)

## Memory layout
I am designing this with 1MByte = 8Mbit flash in mind.  
Many actual devices that can be bought nowadays have 1MB.  
The original way to think about OTA is that the user-code AND the OTA code would be together in one image, occupying < 496kB.
Like this they could swap between the two of them. And if you get away with an user-app that is that small that it all fits, that is great.  
However, in real life this does not work. SSL based downloading is very bulky in both RAM and Flash.  
The proposed scheme means that we can have two boot-slots and we need the system param area.
HOWEVER, they do not need to be at the 512kB boundary.
IF (very likely, but to be confirmed) the flash size of the OTA image is 400kB or less, then there is like 600kB left for the user code.
AND the user code does not have to be inflated by the SSL code of the OTA.  
Another advantage is that the user code will always run from slot0 so the esp-open-rtos Make system doesn't have to be changed for user code.

# Improvements
I will use the [issues section](https://github.com/HomeACcessoryKid/ota/issues) for all to feed back your ideas. I have one issue myself already...

# License

Copyright 2018 HomeACcessoryKid - HacK - homeaccessorykid@gmail.com

Licensed under the Apache License, Version 2.0 (the "License");  
you may not use this file except in compliance with the License.  
You may obtain a copy of the License at  

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software  
distributed under the License is distributed on an "AS IS" BASIS,  
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  
See the License for the specific language governing permissions and  
limitations under the License.
