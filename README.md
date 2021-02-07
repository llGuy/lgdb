# lGdb

A Windows C/C++ Debugger project which contains 3 things:

- An easy to use API for interfacing with Windows debugging utilities (code in `source/lgdb`)
- Debugger through a command line interface (code in `source/lgdbcli`)
- A test program (code in `source/lgdbtest`)
- (COMING SOON) Debugger through a graphical user interface (code in `source/lgdbgui`)

## Setup

- Install [CMake](https://cmake.org/download/) - preferably make it available in your PATH
- Make a build directory in the root of this repo (where CMakeLists.txt is located)
- If you wish to use the command line, `cd build` and then run `cmake ..`
- If you wish to use the GUI, set the `Where to build the binaries` to `the\path\to\project`\build, and where the source file is located to `the\path\to\project`, then click and `Configure` and `Generate`
