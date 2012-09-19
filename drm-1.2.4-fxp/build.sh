#!/bin/sh
make clean
perl config_linux.pl
./config_linux_soc
make
