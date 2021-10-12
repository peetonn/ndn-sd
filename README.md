# ndn-sd

NDN Service Discovery over DNS-SD.

This repository contains code for the following modules / applications:

* `ndn-sd` -- provides an API for announcing NDN microforwarder services over mDNS DNS-SD (Bonjour). This module was designed to have no dependencies on NDN.
It allows peers to announce their data prefix certificate for authentication by other peers.
* `ndnshare` -- an **NFD-free** application that demonstrates use of `ndn-sd` and ndn-ind MicroForwarder for automatic link-local NDN connectivity. Provides functionality for sharing files between local peers.


# Build

Clone repository recursively (for cloning submodules as well):

```
git clone https://github.com/peetonn/ndn-sd.git --recursive
cd ndn-sd
```

## Dependencies

> ❗️ `ndn-sd` and supporting demo app `ndnshare` depend on a number of third-party libraries that need to be compiled and/or installed using package manager for your OS. The instructions below are only suggestions for build process. If you encounter problems compiling any of them, you shall consult original code repositories of these libraries. Some of the dependent libraries have their own dependencies which need to be installed -- original code repos shall be consulted for that.

* [ndn-ind](https://github.com/remap/ndn-ind/tree/app-forwarder) (REMAP fork, branch `app-forwarder`) -- a Named Data Networking client library for C++;
* [cnl-cpp](https://github.com/remap/cnl-cpp) -- a Common Name Library for C++;
* [Bonjour](https://developer.apple.com/download/all/?q=Bonjour%20SDK%20for%20Windows) (Windows) -- [zero-configuration networking](https://developer.apple.com/bonjour/) SDK for Windows;
* [docopt.cpp](https://github.com/docopt/docopt.cpp.git) -- C++11 port of docopt library for python (command-line argument parsing);
* [fmt](https://github.com/fmtlib/fmt) -- {fmt} is an open-source formatting library providing a fast and safe alternative to C stdio and C++ iostreams;
* [cli](https://github.com/daniele77/cli) -- a cross-platform header only C++14 library for interactive command line interfaces;
* [spdlog](https://github.com/gabime/spdlog) -- header-only/compiled, C++ logging library.

### macOS

> ❗️ Each step below assumes you stand in `ndn-sd` root folder.

* ndn-ind
> ❗️ Make sure [ndn-ind prerequisites](https://github.com/remap/ndn-ind/blob/app-forwarder/INSTALL.md#1095-os-x-10102-os-x-1011-macos-1012-macos-1013-and-macos-1014) are installed (openssl and protobuf are required for cnl-cpp).

```
mkdir -p thirdparty/ndn-ind/build && cd thirdparty/ndn-ind
./configure --prefix=`pwd`/build ADD_CFLAGS=-I/usr/local/opt/openssl/include ADD_CXXFLAGS=-I/usr/local/opt/openssl/include ADD_LDFLAGS=-L/usr/local/opt/openssl/lib
make && make install
```

* cnl-cpp

```
mkdir -p thirdparty/cnl-cpp/build && cd thirdparty/cnl-cpp
./configure --prefix=`pwd`/build ADD_CFLAGS=-I/usr/local/opt/openssl/include ADD_CXXFLAGS="-I`pwd`/../ndn-ind/build/include -I/usr/local/opt/openssl/include" ADD_LDFLAGS="-L`pwd`/../ndn-ind/build/lib -L/usr/local/opt/openssl/lib"
make && make install
```

* docopt.cpp

```
mkdir -p thirdparty/docopt.cpp/build && cd thirdparty/docopt.cpp/build
cmake .. -DCMAKE_INSTALL_PREFIX=`pwd`
make && make install
```

* {fmt}

```
mkdir -p thirdparty/fmt/build && cd thirdparty/fmt/build
cmake .. -DCMAKE_INSTALL_PREFIX=`pwd`
make && make install
```

* spdlog
```
brew install spdlog
```

### Windows

1. Install [vcpkg](https://vcpkg.io/en/index.html);
2. Install prerequisites for ndn-ind and ndn-sd:

```
vcpkg install openssl:x64-Windows protobuf:x64-Windows zlib:x64-Windows spdlog
```

3. Setup and compile ndn-ind and ndn-ind-tools according to [instructions](https://github.com/remap/ndn-ind/blob/app-forwarder/VisualStudio/Visual-Studio-README.md);
4. docopt: create "build" folder under "thirdparty/docopt.cpp" then compile using `cmake ..` & Visual Studio;
5. {fmt}: create "build" folder under "thirdparty/fmt" then compile using `cmake ..` &  Visual Studio.

## ndn-sd

### macOS
```
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=`pwd`
make && make install
```

### Windows

1. Create "build" folder in ndnsd root and cd into int using terminal;
2. Assuming vcpkg is installed at "C:\vcpkg":

```
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```
3. Open Visual Studio solution file and build solution.


# How To Use
TBD

## ndn-sd

## ndnshare

Once compiled, run app as follows (certificates are TBD):
```
bin/ndnshare <path_to_folder> <prefix> --cert=some.cert
```

See default output for usage.

