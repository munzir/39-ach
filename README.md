## OVERVIEW

Ach is an Inter-Process Communication (IPC) mechanism and library. It
is especially suited for communication in real-time systems that
sample data from physical processes. Ach eliminates the Head-of-Line
Blocking problem for applications that always require access to the
newest message. Ach is efficient, robust, and formally verified. It
has been tested and demonstrated on a variety of physical robotic
systems. Source code for Ach is available under an Open Source
BSD-style license.

## Installation

    mkdir build
    cd build
    cmake ..
    sudo make install
    sudo ldconfig

## Uninstall
 To remove system files created by the installation of this repo.

    sudo make uninstall

## Old Readme

See the INSTALL file for details.

Alternatively, if you are using Debian or Ubuntu, you can use the golems.org APT repo.

1. Add the following to /etc/apt/sources:

    deb http://code.golems.org/debian squeeze golems.org

  (substitute the codename for your distribution in place of squeeze,
  e.g. wheezy, precise)

2. `sudo apt-get update && sudo apt-get install libach-dev ach-utils`

3. `sudo dpkg-reconfigure ach-utils`

DOCUMENTATION
=============
* Manual: `./doc/manual/ach-manual.html` and
          http://golems.github.com/ach/manual
* API docs: http://golems.github.com/ach/api
* Man Pages: `./doc/man` and
             http://code.golems.org/pkg/ach/man
* Example Code: `./src/ach-example.c`
* Tech Report: http://www.golems.org/node/1526


LANGUAGE BINDINGS
=================
Language bindings are included for Common Lisp, Python, and Java.

* Common Lisp bindings use CFFI and can be loaded with ASDF.

* Python bindings are a C extension module and a Python module 'ach'.
  This can be installed either via `make install` or via Python's
  distutils/pip.

* C++ bindings wrap the C API.
  See `./include/Ach.hpp`

* Java bindings via the Java Native Interface (JNI).
  See `./doc/javadoc` or
       http://golems.github.io/ach/javadoc/

See the INSTALL file for details.


FORMAL MODEL
============
Ach has been formally verified using the SPIN model checker.  This
formal model is included in the source distribution under the `./spin/`
directory.
