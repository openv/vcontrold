# Building via cmake

The Package can be built with the normal cmake procedure. Simply execute the following steps in your source directory:

```
mkdir build
cd build
cmake ..
make
```
To install the package, execute
```
make install
```
If you want to install the package with another prefix, use
```
make DESTDIR=<PREFIX> install
```
