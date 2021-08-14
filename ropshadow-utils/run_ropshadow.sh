#!/bin/bash
echo "$1" | /home/gaby/ropshadow/pin/pin -ifeellucky -injection child -t /home/gaby/ropshadow/ropshadow/obj-intel64/ropshadow.so -- "$2" &> output_ropshadow.txt
