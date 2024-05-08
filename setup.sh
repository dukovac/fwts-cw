#!/bin/bash

sudo apt-get install autoconf automake libglib2.0-dev libtool libpcre3-dev flex bison dkms libfdt-dev libbsd-dev

autoreconf -ivf
./configure
make -j $(nproc)
sudo make install