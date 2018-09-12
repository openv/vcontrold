# Building via cmake

The `vcontrold` package depends on `libxml2`.

For the build process on \*nix machines, usually the `libxml2-dev` package is needed. To get the documentation converted from Restructured Text (similar to Markdown) to the format used by `man`, the `rst2man` utility has to be installed.

The package can be built with the normal cmake procedure. Simply execute the following steps in your source directory:

```
mkdir build
cd build
cmake ..
make
```

### Build options

There are three options for the build process with their defaults:

* _MANPAGES=ON_ Build man pages via `rst2man`
* _VCLIENT=ON_  Build the `vclient` helper program (for communication with vcontrold)
* _VSIM=OFF_ Build the `vsim` helper program (for development and testing purposes)

The installation path can be altered by
 
 * _CMAKE_INSTALL_PREFIX=`/usr/local`_
   This directory is prepended onto all install directories. This variable defaults to `/usr/local` on UNIX and `c:/Program Files` on Windows

Invocation is as follows:

```
cmake -DVSIM=ON -DMANPAGES=OFF -DCMAKE_INSTALLATION_PREFIX=/usr/local ..
```

### Installation

To install the package, execute
```
sudo make install
```
The whole installation can be relocated to a different directory by supplying a `DESTDIR` variable:
```
sudo make DESTDIR=<DESTDIR> install
```
In this case, the entire package will be installed in a directory with the installation prefix prepended with the `DESTDIR` value, which finally gives `<DESTDIR>/<CMAKE_INSTALL_PREFIX>`.
