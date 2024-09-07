# QtNetworkCrumbs

## What's this?

This are some tiny networking toys written in C++17 for [Qt](https://qt.io).

- a [tiny mDNS resolver](mdns/mdnsresolver.h) ([usage demo](mdns/mdnsresolverdemo.cpp))
- a [compressing HTTP/1.1 server](http/compressingserver.cpp)

## What's needed?

- [CMake 3.10](https://cmake.org/)
- [Qt 6.5](https://qt.io), or Qt 5.15
- C++17

## What's supported?

This is build and tested on all the major platforms supported by Qt:
Android, iOS, Linux, macOS, Windows.

[![Continuous Integration][build-status.svg]][build-status]

---

Copyright (C) 2019-2024 Mathias Hasselmann
Licensed under MIT License

<!-- some more complex links -->
[build-status.svg]: https://github.com/hasselmm/QtNetworkCrumbs/actions/workflows/integration.yaml/badge.svg
[build-status]:     https://github.com/hasselmm/QtNetworkCrumbs/actions/workflows/integration.yaml
