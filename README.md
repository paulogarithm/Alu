<h1 align=center> Alu </h1>

Alu is an interpretted programming language that uses bytecode.

## How to build

First, clone the repository and enter it
```sh
git clone https://github.com/paulogarithm/Alu
cd Alu
```

### Using CMake
You will need:
- CMake
- GCC

Copy paste the commands bellow:
```sh
mkdir build
cd build
cmake ..
make
```
The binary is in `./build/alu`.

### Using ninja
You will need:
- NinjaBuild
- GCC

Just run:
```sh
ninja
```
The binary is in `./alu`.
