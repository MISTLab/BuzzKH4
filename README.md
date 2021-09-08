## Installation

1- Download the khepera light toolchain: http://ftp.k-team.com/KheperaIV/software/light_tools/khepera4-yocto-light-kb1.0.tar.bz2
   Note: This version of the toolchain is deprecated. Contact us for more information.

2- Install it: 

`bash sudo tar -xjf khepera4-yocto-light-kbX.Y.tar.bz2 -C /usr/local`

3 - Get this repo:

`bash git clone https://github.com/MISTLab/BuzzKH4.git`

4 - Get and cross-compile Buzz: 

```bash 
git clone https://github.com/MISTLab/Buzz.git

cd Buzz && cp ../BuzzKH4/src/cmake/TargetKheperaIV.cmake src/cmake/

mkdir build && cd build/

cmake -DCMAKE_TOOLCHAIN_FILE=../src/cmake/TargetKheperaIV.cmake -DCMAKE_MAKE_PROGRAM=make -DCMAKE_DISABLE_FIND_PACKAGE_ARGoS=TRUE ../src/

make

sudo make install
```

5 - Cross-compile this repo (assuming it is downloaded from previous steps):

```bash

cd BuzzKH4 && mkdir build && cd build/

cmake -DCMAKE_TOOLCHAIN_FILE=../src/cmake/TargetKheperaIV.cmake -DCMAKE_MAKE_PROGRAM=make ../src/

make

sudo make install
```

6 - upload it to your khepera: `bash scp bzzkh4 K01:/home/root/`
