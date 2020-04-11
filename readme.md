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

* the header-only, _non-Boost_ version of [ASIO framework](http://www.think-async.com)
* [Lua 5.2.3](https://github.com/LuaDist/lua/tree/5.2.3)
* a customized/patched version of [wxWidgets](https://github.com/wxWidgets/wxWidgets/tree/WX_3_0_3_BRANCH)
* a commercial version of [JUCE](https://github.com/WeAreROLI/JUCE)
* system-paclaged (binary) libraries of [Cairo](https://www.cairographics.org)


# Supported platforms

* Microsoft Windows 7-10
* Apple macOS X
* Apple iOS 8+ including simulator
* Linux kernel 3.x with Gtk 2/3 


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


