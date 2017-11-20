#!/bin/bash
export BUZZ_INCLUDE_PATH=/home/root/MIST/include
~/MIST/bin/bzzparse $1.bzz $1.basm
~/MIST/bin/bzzasm $1.basm $1.bo $1.bdb
~/MIST/bin/bzzkh4 tcp 1000 $1.bo $1.bdb
