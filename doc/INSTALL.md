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

The project default installation prefix on UNIX is `/usr`. It installs the
daemon as `/usr/sbin/vcontrold`, the helpers below `/usr/bin`, and man pages
below `/usr/share/man`. This layout matches the service example shipped in
`doc/examples/vcontrold.service`.

The installation path can be altered by

 * _CMAKE_INSTALL_PREFIX=`/usr/local`_
   This directory is prepended onto all install directories. A source-local
   installation therefore uses `/usr/local/sbin/vcontrold` and
   `/usr/local/bin/vclient`; adjust service files accordingly. The platform
   default is `c:/Program Files` on Windows.

Invocation is e.g. as follows:

```
cmake -DVSIM=ON -DMANPAGES=OFF -DCMAKE_INSTALL_PREFIX=/usr/local ..
```

A common approach for a minimal installation (without manpages) would be:

```
cmake -DMANPAGES=OFF ..
```

### Installation

To install the package, execute as root:

```
make install
```

or, on systems that use this logic (e.g. Debian, Ubuntu etc.), use `sudo`:

```
sudo make install
```

The whole installation can be relocated to a different directory by supplying a `DESTDIR` variable:

```
make DESTDIR=<DESTDIR> install
```

or

```
sudo make DESTDIR=<DESTDIR> install
```

respectively. In this case, the entire package will be installed in a directory with the installation prefix prepended with the `DESTDIR` value, which finally gives `<DESTDIR>/<CMAKE_INSTALL_PREFIX>`.

### Service configuration

The systemd example assumes the `/usr` prefix and an XML configuration at
`/etc/vcontrold/vcontrold.xml`. When using `/usr/local`, copy and adapt the
example so `ExecStart` points to `/usr/local/sbin/vcontrold`.

Sending `SIGHUP` reloads device, protocol, command and unit definitions
transactionally. Listener address, port, daemon user/group, PID file and log
destination are established during process startup; restart the service after
changing those values. Back up the active XML files before deploying a new
controller profile.
