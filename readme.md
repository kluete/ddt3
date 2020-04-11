# About

DDT3 is a remote debugger for the Lua language, written in ISO C++14, made of two components:

    stand-alone client application with user interface - running on development PC
    minimal, headless remote component - embedded in debuggee on target device, hooked into Lua runtime

communication happens over TCP/IP.

# Dependencies

Client and/or Daemon components depend on:

* the header-only, _non-Boost_ version of [ASIO framework](http://www.think-async.com)
* [Lua 5.2.3](https://github.com/LuaDist/lua/tree/5.2.3)
* a customized/patched version of [wxWidgets](https://github.com/wxWidgets/wxWidgets/tree/WX_3_0_3_BRANCH)
* a commercial version of [JUCE](https://github.com/WeAreROLI/JUCE)


# Support platforms

* Windows 7+
* macOS 10+
* Apple iOS 8+ including simulator
* Linux kernel 3.x+ with Gtk 2/3 


# Build instructions

The projects won't currently build given the heavy patching of underlying frameworks, especially wxWidgets.

```
git clone https://github.com/kluete/ddt3.git
cd ddt3
git submodule update --init
./build.sh
```

# Invocation


# Famous last words

Enjoy, hope it's useful but, still, "caveat emptor" (_so there_).


