## Installation

1- Download the khepera light toolchain: http://ftp.k-team.com/KheperaIV/software/light_tools/khepera4-yocto-light-kb1.0.tar.bz2

2- Install it: sudo tar -xjf khepera4-yocto-light-kbX.Y.tar.bz2 -C /usr/local

3- get and cross-compile Buzz: 

git clone https://github.com/MISTLab/Buzz.git

cd Buzz && mkdir build && cd build/

cmake -DCMAKE_TOOLCHAIN_FILE=cmake/TargetKheperaIV.cmake ../src/

sudo make install

4- get and cross-compile this repo:

git clone https://github.com/MISTLab/BuzzKH4.git

cd BuzzKH4 && mkdir build && cd build/

cmake -DCMAKE_TOOLCHAIN_FILE=cmake/TargetKheperaIV.cmake ../src/


5- upload it to your khepera: scp bzzkh4 K01:/home/root/
