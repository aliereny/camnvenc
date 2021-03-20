# CAMNVENC

Camnvenc is a qualification task for CCExtractor'21 \
(https://ccextractor.org/public/gsoc/takehome/#interprocess_communication_low-level).\
This project focuses on the problem of encoding stream of 10 or more cameras using NVDIA hardware. CUDA toolkit and FFMpeg is used to create 2 simultaneous encoding sessions of nvenc. Interprocess communication is also used to reduce load of memory copying.  

## How Does It Work?

This project consists of two programs, cammanager and gpumanager.\
 Gpumanager runs independent of cammanagers. It detects recently active cameras and recently closed/crashed cameras continuously. When a camera has 60 frames ready, it starts encoding. As it encodes first 60 frames of a camera, it sends that camera end of the queue and starts encoding next camera. This gpumanager can work with 1 to 10 or more cameras.\
Cammanagers are individual processes that reads random data (for test case) and writes them into shared memory as it conducts a queue data structure. They can be activated or deactivated independent of any other process.

## Dependencies
build-essentials (for make and gcc)
```bash
$ sudo apt-get install build-essential
```
pkg-config
```bash
$ sudo apt-get install pkg-config
```
libssl-dev
```bash
$ sudo apt-get install libssl-dev
```
libavformat-dev
```bash
$ sudo apt-get install libavformat-dev
```
CUDA Toolkit

```
https://docs.nvidia.com/cuda/cuda-installation-guide-linux/index.html
```
## Installing

```bash
$ git clone https://github.com/aliereny/camnvenc.git
$ make
```
## Building
This project is designed for Linux environment. To build
1. Move to corresponding directory:
```bash
$ cd camnvenc
```
2. Run makefile 
```bash
$ make
```
## Testing
1. A gpumanager and different cameras should be run:
```bash
$ ./gpumanager
```
2. Cameramanager's should be run on different terminals with following command:
```bash
$ ./cammanager /camera_<id>
```
id should be in range 0-10, for example:
```bash
$ ./cammanager /camera_0
```
or
```bash
$ ./cammanager /camera_1
```
3. Automatic generated "log.txt" file can be used to see processes working correctly. log.txt file will look like:
```txt
camera=/camera_0 frame=    211 checksum=89364006428aed82b5389a57b7ccd2fe
camera=/camera_0 frame=    212 checksum=4426bb88fe5aef020d45f13f31c5b6f8
camera=/camera_0 frame=    213 checksum=53f5b169c938b506f54f8fc80d244646
```
## License
[MIT](https://choosealicense.com/licenses/mit/)
