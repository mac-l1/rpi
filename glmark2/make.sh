#!/bin/bash

# install some basic build tools 
sudo apt-get install -y --force-yes build-essential autoconf libtool pkg-config xutils-dev

# configure and build
./waf configure --enable-glesv2 /home/firefly/rpi/glmark2/data --prefix=/usr/local/
./waf
sudo ./waf install --destdir=/
