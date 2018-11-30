# MacOS

## Prerequisites for OSX (Sierra):
- Qt (used 5.6.2)
- XCode with MacPorts
- HackRF One

We are only covering HackRF One, since is only the hardware I own.
In the future we will test RTL based: NooElec NESDR Nano 3(RTL2832U & R820T2).

### Project dir structure:
SDRangel-3.x:
  + build-sdrangel.macos-Desktop_Qt_5_6_2_clang_64bit-Release
  + sdrangel
  + deps
    + cm256cc
    + dsdcc
    + mbelib
+ boost_1_64_0/

### Environment preparation
Boost 1.64: Download and unpack
There are a few dependencies which can be installed through MacPorts:
```
sudo port install cmake hackrf-devel bladeRF rtl-sdr opencv
```

Clone other libs to deps folder:

##### cm256cc:
```
git clone https://github.com/f4exb/cm256cc.git

```


##### mbelib:
```
git clone https://github.com/szechyjs/mbelib.git

```

##### dsdcc:
```
git clone https://github.com/f4exb/dsdcc.git

```


## Build
Release build configuration with QT Creator

## Deployment
Go into release build directory, something like: ```build-sdrangel.macos-Desktop_Qt_***Release```
and run deployment script:
```../sdrangel/apple/deploy.sh```

## Run
from build directory:
```DYLD_LIBRARY_PATH=/opt/local/lib:.; ../MacOS/sdrangel```
or
```../sdrangel/apple/run.sh```
