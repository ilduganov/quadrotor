#!/bin/sh
make clean
make -f Makefile2 clean
make
make -f Makefile2
grep -v ":00000001FF" regulator.hex > program.hex
grep -v ":0400000" boot.hex >> program.hex
avrdude -p atmega88p -P /dev/parport1 -c stk200 -B 57600 -U flash:w:program.hex
