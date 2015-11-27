#!/bin/bash
./waf configure --enable-glesv2 /home/firefly/rpi/glmark2/data --prefix=/usr/local/
./waf
sudo ./waf install --destdir=/
