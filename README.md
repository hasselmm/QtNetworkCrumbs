# QtNetworkCrumbs

## What is this?

This are some tiny networking toys for [Qt][qt-opensource] based applications, written in C++17:

* [A minimal mDNS-SD resolver](#a-minimal-mdns-sd-resolver)
* [A minimal SSDP resolver](#a-minimal-ssdp-resolver)
* [A declarative XML parser](#a-declarative-xml-parser)

### A minimal mDNS-SD resolver

[DNS Service Discovery][DNS-SD], also known as "DNS-SD" and "Bonjour", is an efficient method to discover
network services based. Using [Multicast DNS][mDNS] it's a "Zeroconf" technique for local networks. Via
traditional [Unicast DNS][DNS] it also works over the Internet.

With this library discovering DNS-SD services is as simple as this:

```C++
const auto resolver = new mdns::Resolver;

connect(resolver, &mdns::Resolver::serviceFound,
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

### A declarative XML parser

Parsing [XML documents][XML] can is pretty annoying.
Maybe it is annoying less for you if you got a decent declarative XML parser at your hand.
The declarative XML parser provided here easily turns such XML

```XML
<?xml version="1.0"?>
<root>
  <version>
    <major>1</major>
    <minor>2</minor>
  </version>

  <icons>
    <icon>
      <mimetype>image/png</mimetype>
      <width>384</width>
      <height>256</height>
      <url>/icons/test.png</url>
    </icon>

    <icon>
      <mimetype>image/webp</mimetype>
      <width>768</width>
      <height>512</height>
      <url>/icons/test.webp</url>
    </icon>
  </icons>

  <url>https://ecosia.org/</url>
  <url>https://mission-lifeline.de/en/</url>
</root>
```

into such C++ data structures

```C++
struct TestResult
{
    struct Icon
    {
        QString mimeType = {};
        QSize   size     = {};
        QUrl    url      = {};
        QString relation = {};
    };

    QVersionNumber version = {};
    QList<Icon>    icons   = {};
    QList<QUrl>    urls    = {};
};
```

with as little as that:

```C++
auto result = TestResult{};
const auto states = StateTable {
    {
        State::Document, {
            {u"root",       transition<State::Root>()},
        }
    }, {
        State::Root, {
            {u"version",    transition<State::Version>()},
            {u"icons",      transition<State::IconList>()},
            {u"url",        append<&TestResult::urls>(result)},
        }
    }, {
        State::Version, {
            {u"major",      assign<&TestResult::version, VersionSegment::Major>(result)},
            {u"minor",      assign<&TestResult::version, VersionSegment::Minor>(result)},
        }
    }, {
        State::IconList, {
            {u"icon",       transition<State::Icon, &TestResult::icons>(result)},
        }
    }, {
        State::Icon, {
            {u"mimetype",   assign<&TestResult::Icon::mimeType>(result)},
            {u"width",      assign<&TestResult::Icon::size, &QSize::setWidth>(result)},
            {u"height",     assign<&TestResult::Icon::size, &QSize::setHeight>(result)},
            {u"url/@rel",   assign<&TestResult::Icon::relation>(result)},
            {u"url",        assign<&TestResult::Icon::url>(result)},
        }
    }
};

parse(lcExample(), State::Document, {{xmlNamespace, states}});
```

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
[XML]:              https://en.wikipedia.org/wiki/XML
