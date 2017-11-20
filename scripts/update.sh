#!/bin/bash

# constants
KH_ENV_LOC='/home/mistlab/khepera-proper/'
KH_ENV='/home/root/MIST'

# Usage ./update.sh (all|kh4) K01 K02 ... K_n
OPTION=$1
shift

if [ $OPTION == "all" ]
then
  for khepera in "$@"
  do
    echo "Updating (complete MIST workspace) ... $khepera"
    # BIN
    # create bin and copy executables
    ssh $khepera "mkdir -p $KH_ENV/bin"
    find /usr/local/khepera4-yocto/build/tmp/sysroots/i686-linux/usr/bin/ -executable -type f -name 'bzz*' -exec scp {} $khepera:$KH_ENV/bin/ \;
    scp $KH_ENV_LOC/BuzzKH4/build/bzzkh4 $khepera:$KH_ENV/bin/
    #scp /usr/local/khepera4-yocto/build/tmp/sysroots/i686-linux/usr/bin/bzzc $khepera:$KH_ENV/bin/

    # DOC
    # create doc and copy readme
    ssh $khepera "mkdir -p $KH_ENV/doc/buzz"
    scp -r /usr/local/doc/buzz/* $khepera:$KH_ENV/doc/buzz/

    # INCLUDE
    # create include and copy headers
    ssh $khepera "mkdir -p $KH_ENV/include/buzz"
    scp -r /usr/local/include/buzz/* $khepera:$KH_ENV/include/buzz/
    scp /usr/local/share/buzz/include/*.bzz $khepera:$KH_ENV/include/

    # LIB
    # create lib and copy .so
    ssh $khepera "mkdir -p $KH_ENV/lib"
    scp /usr/local/khepera4-yocto/build/tmp/sysroots/i686-linux/usr/lib/libbuzz*.so $khepera:$KH_ENV/lib/

    # SHARE
    # create lib and copy misc
    ssh $khepera "mkdir -p $KH_ENV/share/"
    scp -r /usr/local/khepera4-yocto/build/tmp/sysroots/i686-linux/usr/share/man $khepera:$KH_ENV/share/
    scp -r /usr/local/share/buzz/ $khepera:$KH_ENV/share/
  done
elif [ $OPTION == "kh4" ]
then
  for khepera in "$@"
  do
    echo "Updating (bzzkh4 only) ... $khepera"
    # BIN
    # create bin and copy executables
    ssh $khepera "mkdir -p $KH_ENV/bin"
    scp $KH_ENV_LOC/BuzzKH4/build/bzzkh4 $khepera:$KH_ENV/bin/
  done
else
  echo "Usage: ./update.sh (all|kh4) K01, K02, ..., K_n"
fi
