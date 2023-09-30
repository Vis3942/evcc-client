# README

KITU SMART ENERGY 2

Copyright (C) 2020 Kitu Systems, Inc. All rights reserved.
This source code and any compilation or derivative thereof is the proprietary
information of Kitu Systems, Inc and is confidential in nature.


This is the README for the Kitu Spark Server relase for Eaton

## Submodules
Stellantis and Eaton are using same Stack branch:
stack: release/eaton

## Package content

This release package contains source code for building user application process;
and KITU SL API Process binary file.

./
       ./app                                                   # Application process example code
              ./include
              ./source
       CMakeLists.txt                                          # cmake file to build app
       ./document                                              # SL_API document
       ./kitu_api                                              # KITU APIs
              ./include
              ./source
       kituclient_process                                      # Application process binaray (pre-build)
       kitusl_process_1.0.0_b7ac2da6_rpi                       # Kitu SL API process (used by the app process)
       README.md                                               # Readme
       sl_api_process -> kitusl_process_1.0.0_b7ac2da6_rpi     # Symbolic link of KITU SL API Process, must have


## Building

Ensure that cmake 2.8+ is installed
```bash
$ mkdir build && cd build
$ cmake ..
$ make
```

## Running the server

When passing the -h parameter to *kituserver_VERSION_HASH_lnx*, here are the following command line options you may pass:

```
Usage: ./build/kituclient_process {-s <security file>} {-r <rootfile>} {-c <pemCertChainFile>} {-d <basicECPrivKeyFile>}
          {-i <interface>} {-p <path to config>} {-g <port number>}
          {-t <HTTP port>} {-T <HTTPS port>}
   or: ./build/kituclient_process {--secfile=<security file>} {--rootfile=<x>} {--pemCertChainFile=<x>} {--basicECPrivKeyFile=<x>}
          {--interface=-<interface>} {--configpath=<path to config>} {--g2hterm=<port number>}
          {--http=<HTTP port>} {--https=<HTTPS port>}

   where:
        <secfile> - basename of the pem cert chain and pem private key file (e.g., /opt/kitu/devcertinfo)
        <rootfile> - filename of the pem Root CA certificate file (e.g., /opt/kitu/rootcacert.pem)
        <pemCertChainFile> - PEM certificate chain file
        <basicECPrivKeyFile> - Basic EC private key file
        <interface>      - the interface name for SEP communications (e.g., wlan0)
        <path to config> - path to the directory containing configuration files
        <port number>    - the port number with which to connect G2H_Terminal
        <HTTP port> - HTTP port assigned to embedded server, default is 80
        <HTTPs port> - HTTPs port assigned to embedded server, default is 443
```

Note that the -p parameter is not currently implemented.

The certificate information is built-in to the executable already. If you wish to specify different certificates you may use the command line options to do so. Below is an example:

```bash
./kituclient_process -i eth0 -r /path/to/rootcacert.pem -c /path/to/certchain.p7b
```

* Generic Query
* Subtype Queries
  * Time
  * Self-Device
  * Usage Point
