# Building via cmake

The Package can be built with the normal cmake procedure. Simply execute the following steps in your source directory:

```
mkdir build
cd build
cmake ..
make
```

### Build options

There are three options for the build process with their defaults:

* _MANPAGES=ON_ Build man pages via rst2man
* _VCLIENT=ON_  Build the vclient helper program (for communication with vcontrold)
* _VSIM=OFF_ Build the vsim helper program (for development and testing purposes)

Invokation is as follows;

```
cmake -DVSIM=ON -DMANPAGES=OFF ..
```

### Installation

To install the package, execute
```
sudo make install
```
If you want to install the package with another prefix you can either use
```
sudo make DESTDIR=<PREFIX> install
```
or pass the desired prefix to the `cmake` command before via
```
cmake -DCMAKE_INSTALL_PREFIX=<PREFIX> ..
```
