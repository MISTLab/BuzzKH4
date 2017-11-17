## About

BuzzKH4 is a [Buzz](http://the.swarming.buzz/) implementation for Khepera IV robots.

### Prerequisites

The following instructions are made for Ubuntu 16.04 LTS.

* [Khepera Light Toolchain](http://ftp.k-team.com/KheperaIV/software//Gumstix%20COM/light_tools/) - CrossCompiler, select the one for Yocto (v.1.0)
* Make sure to install gcc-multilib:

```
sudo apt-get install gcc-multilib
```

To have a cleaner workspace, it is suggested to creata folder, e.g. khepera-proper, and inside, clone:
* [Buzz](https://github.com/MISTLab/Buzz)
* [BuzzKH4](https://github.com/MISTLab/BuzzKH4)

## Installation

In order to sucessfully compile the project, you need a cross compiler for Yocto linux and [Khepera IV](https://www.k-team.com/khepera-iv) API.

1. Install Khepera IV Light Toolchain:

```
sudo tar -xjf khepera4-yocto-light-kbX.Y.tar.bz2 -C /usr/local
```

2. Create and navigate to a build folder, then cross-compile and install Buzz (note, TargetKheperaIV.cmake is located in BuzzKH4/src/cmake folder):

```
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/TargetKheperaIV.cmake ../src/
sudo make install
```

3. Cross compile BuzzKH4 (create and navigate to a build folder):

```
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/TargetKheperaIV.cmake ../src/
```

4. Assuming everything is compiled without errors, update.sh can be used to deploy the necessary binaries to a Khepera robot, assuming that the folder strcture is given as (~/khepera-proper/):

```
.
..
Buzz
BuzzKH4
update.sh
```

Usage (if necessart, add executable permission as: chmod +x update.sh):
```
./update.sh (all|kh4) K01, K02, ..., K_n
```
