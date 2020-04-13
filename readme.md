# About

DDT3 is a remote debugger for the Lua language, written in ISO C++14. It is made of two components:

1. stand-alone client application with user interface - running on development computer
2. minimal, headless remote component - embedded in debuggee on target device, hooked into Lua runtime

communication happens over TCP and UDP.


# Demo

You'll find a *fancy, overly verbose* description and screencast of this project over at [DDT3](http://www.laufenberg.ch/ddt3). Building the entire code wil be a challenge, though.


# But Franck... Why???

The point of this repository is first and foremost to showcase my programming style... as Lua has largely been supplanted by higher-level (slower) languages.


# Dependencies

This project depends on:

# repos

* [Lua 5.2.3](https://github.com/LuaDist/lua/tree/5.2.3)
* a forket, customized version 3.0.3 of [wxWidgets](https://github.com/kluete/wxWidgets.git)
* a commercial version of [JUCE](https://github.com/WeAreROLI/JUCE.git)							// used in ddt or just lxMusic ?

* the header-only, _non-Boost_ version of [ASIO framework](http://www.think-async.com)
* my own [utilities](https://github.com/kluete/lxUtils.git)
* unit test framework [Catch](https://github.com/philsquared/Catch.git)

## system-packaged libs																// no longer x-platform!!!
* [Cairo](https://www.cairographics.org)
* font rendering SDK [freetype](https://git.sv.gnu.org/freetype/freetype2.git)
* apt-get install libcrypto++-dev													// used for what?? hash?


# Supported platforms

* Microsoft Windows 7-10
* Apple macOS X
* Apple iOS 8+ including simulator
* Linux kernel 3.x with Gtk 2/3 


# 3rd PARTY

```
git submodule add https://github.com/kluete/wxWidgets.git ./thirdparty/wxWidgets
git submodule add https://github.com/LuaDist/lua/tree/5.2.3 ./thirdparty/lua

```

# Build instructions

The projects won't currently build, given the heavy patching of underlying frameworks, especially on wxWidgets.




```
git clone https://github.com/kluete/ddt3.git
cd ddt3
git submodule update --init
./build.sh

***
git clone git://anongit.freedesktop.org/git/cairo
git clone git://anongit.freedesktop.org/git/pixman.git

```

# Invocation


# Famous last words

Enjoy, hope it's useful but, still, "caveat emptor" (_so there_).


