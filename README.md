# QtNetworkCrumbs

## What is this?

This are some tiny networking toys for [Qt][qt-opensource] based applications, written in C++17.

### A minimal mDNS-SD resolver

[DNS Service Discovery][DNS-SD], also known as "DNS-SD" and "Bonjour", is an efficient method to discover
network services based. Using [Multicast DNS][mDNS] it's a "Zeroconf" technique for local networks. Via
traditional [Unicast DNS][DNS] it also works over the Internet.

With this library discovering DNS-SD services is as simple as this:

```C++
const auto resolver = new mdns::Resolver;

connect(resolver, &mdns::Resolver::serviceResolved,
        this, [](const auto &service) {
    qInfo() << "mDNS service found:" << service;
});

resolver->lookupServices({"_http._tcp"_L1, "_googlecast._tcp"_L1});
```

The C++ definition of the mDNS resolver can be found in [mdnsresolver.h](mdns/mdnsresolver.h).
A slightly more complex example can be found in [mdnsresolverdemo.cpp](mdns/mdnsresolverdemo.cpp).

### A minimal SSDP resolver

The [Simple Service Discovery Protocol (SSDP)][SSDP] is another "Zeroconf" technique based on multicast messages.
It's also the basis [Univeral Plug and Play (UPnP)][UPnP].

With this library discovering SSDP services is as simple as this:

```C++
const auto resolver = new ssdp::Resolver;

connect(resolver, &ssdp::Resolver::serviceFound,
        this, [](const auto &service) {
    qInfo() << "SSDP service found:" << service;
});

resolver->lookupService("ssdp:all"_L1);
```

The C++ definition of the SSDP resolver can be found in [ssdpresolver.h](ssdp/ssdpresolver.h).
A slightly more complex example can be found in [ssdpresolverdemo.cpp](ssdp/ssdpresolverdemo.cpp).

### A compressing HTTP server

This library also contains a very, very minimal [compressing HTTP/1.1 server](http/compressingserver.cpp).
More a demonstration of the concept, than a real implementation.

## What is needed?

- [CMake 3.10](https://cmake.org/)
- [Qt 6.5][qt-opensource], or Qt 5.15
- C++17

## What is supported?

This is build and tested on all the major platforms supported by Qt:
Android, iOS, Linux, macOS, Windows.

[![Continuous Integration][build-status.svg]][build-status]

---

Copyright (C) 2019-2024 Mathias Hasselmann
Licensed under MIT License

<!-- some more complex links -->
[build-status.svg]: https://github.com/hasselmm/QtNetworkCrumbs/actions/workflows/integration.yaml/badge.svg
[build-status]:     https://github.com/hasselmm/QtNetworkCrumbs/actions/workflows/integration.yaml

[qt-opensource]:    https://www.qt.io/download-open-source

[DNS-SD]:           https://en.wikipedia.org/wiki/Zero-configuration_networking#DNS-SD
[DNS]:              https://en.wikipedia.org/wiki/Domain_Name_System#DNS_message_format
[mDNS]:             https://en.wikipedia.org/wiki/Multicast_DNS
[SSDP]:             https://en.wikipedia.org/wiki/Simple_Service_Discovery_Protocol
[UPnP]:             https://en.wikipedia.org/wiki/Universal_Plug_and_Play
