# eFCrawler

**Easy Flexible Crawler** — autonomous research and resource discovery for Linux.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)](#requirements)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](#technology)
[![Qt 6](https://img.shields.io/badge/Qt-6-green.svg)](#technology)

---

## Overview

eFCrawler is a native Linux desktop application that automates online research.

Give it a topic, and it performs structured searches, collects the resources it
finds, classifies them by type, and provides direct access to web pages and
downloadable documents such as PDF files — all in a single organized interface.

Research normally means repeating variations of the same query by hand, opening
results one at a time, and losing track of what you have already seen. eFCrawler
automates that discovery loop so you can focus on the material rather than the
searching.

Built with C++23 and Qt6. Free and open source.

> **Screenshots** — *[add a screenshot of the main window here. This is the single
> biggest improvement you can make to this README.]*

---

## Features

**Research**
- Autonomous multi-query research from a single topic
- Multiple research-provider architecture with automatic fallback — research
  continues when a provider is unavailable
- Free-resource filtering
- Pause, resume, and stop active research

**Discovery**
- Web resource discovery
- PDF resource discovery
- Resource classification (WEB, PDF, Document, Audio, Video)
- Duplicate suppression across queries

**Access**
- Open web and PDF resources directly from the results list
- Download PDF resources from within the application
- Dedicated download directory with one-click access

**Interface**
- Dark and light themes
- Native Qt6 desktop interface

---

## How it works

eFCrawler separates research planning from resource retrieval, so providers can
be added or replaced without changing the rest of the application.

```
                    ┌─────────────────┐
                    │      User       │
                    └────────┬────────┘
                             │  topic
                    ┌────────▼────────┐
                    │ ResearchEngine  │  plan · deduplicate · filter
                    └────────┬────────┘
                             │  query
                    ┌────────▼────────┐
                    │ ProviderManager │  fallback chain
                    └────────┬────────┘
                             │
              ┌──────────────┴──────────────┐
              │                             │
    ┌─────────▼─────────┐       ┌───────────▼────────┐
    │  DuckDuckGo       │       │  Wikipedia         │
    │  (primary)        │       │  (fallback)        │
    └───────────────────┘       └────────────────────┘
```

### Research providers

| Provider | Role |
| --- | --- |
| **DuckDuckGo** | Primary general-purpose web and resource discovery |
| **Wikipedia** | Fallback provider with external-resource discovery |

The provider abstraction is deliberately extensible. Additional providers can be
introduced without modifying the research engine or the interface.

### Resource types

eFCrawler distinguishes between ordinary web resources and directly accessible
downloadable resources.

| Type | Description |
| --- | --- |
| `WEB` | Web pages and articles |
| `PDF` | Directly accessible PDF documents |
| `Document` | Other document formats |
| `Audio` | Audio resources |
| `Video` | Video resources |

PDF resources can be opened directly or downloaded from within the application.

---

## Requirements

- **Operating system** — Linux, Debian/Ubuntu family (developed and tested on
  Pop!_OS)
- **Compiler** — a C++23-capable compiler (GCC 13+ or Clang 17+; older compilers
  may require `-std=c++2b`)
- **CMake** — 3.16 or newer
- **Qt** — Qt6 with the Widgets and Network modules

---

## Build

### 1. Install dependencies

On Debian/Ubuntu-derived systems:

```bash
sudo apt install build-essential cmake qt6-base-dev
```

### 2. Clone the repository

```bash
git clone https://github.com/drericflores/efcrawler.git
cd efcrawler
```

### 3. Configure

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

### 4. Build

```bash
cmake --build build -j2
```

### 5. Run

```bash
./build/efcrawler
```

---

## Install

The project provides a CMake installation target for the eFCrawler executable.

```bash
sudo cmake --install build
```

Linux desktop integration — including the application icon and desktop launcher
— is being incorporated into the installation system.

---

## Usage

1. **Enter a topic** in the research field.
2. **Start research.** eFCrawler performs a structured series of searches and
   streams results into the results table as they are found.
3. **Manage the run** at any time:
   - **Pause** — the current request finishes; no new request starts
   - **Resume** — continue from where research stopped
   - **Stop** — end the run; results found so far are retained
4. **Open a resource** by selecting it and choosing **Open**.
5. **Download a PDF** by selecting it and choosing **Download**.
6. **Access your downloads** via the **Downloads Folder** command.

### Where downloads go

Downloaded resources are stored in the `eFCrawler` directory inside your
standard Linux Downloads location:

```
~/Downloads/eFCrawler/
```

---

## Project structure

```
eFCrawler/
├── CMakeLists.txt
├── README.md
├── resources/
│   └── efcrawler.png
└── src/
    ├── main.cpp
    ├── core/
    │   ├── ResearchEngine.cpp
    │   └── ResearchEngine.hpp
    ├── model/
    │   └── SearchResult.hpp
    └── providers/
        ├── SearchProvider.hpp
        ├── DuckDuckGoProvider.cpp
        ├── DuckDuckGoProvider.hpp
        ├── ProviderManager.cpp
        ├── ProviderManager.hpp
        ├── WikipediaProvider.cpp
        └── WikipediaProvider.hpp
```

---

## Technology

| Component | Used for |
| --- | --- |
| **C++23** | Implementation language |
| **Qt6** | Graphical interface and networking |
| **Qt Widgets** | Desktop interface |
| **Qt Network** | Research requests and downloads |
| **CMake** | Build system |

---

## Project status

eFCrawler is under active development.

**Current version: 0.2.3**

The current release has been tested for application startup, repeated research
operations, provider fallback, WEB and PDF discovery, opening resources, direct
PDF downloading, pause and resume, stop, free-resource filtering, download-folder
access, clean shutdown, restart, and post-restart research.

### Roadmap

- Expanded media discovery — audio, video, and image resource types
- Additional research providers
- Result metadata including file size and licence information
- Automated test suite and continuous integration
- First tagged release

---

## License

Copyright © 2026 Dr. Eric O. Flores.

eFCrawler is free software, distributed for educational and research purposes.
You may use, study, modify, and redistribute it under the terms of the
**GNU General Public License, version 3 or later**. See [LICENSE](LICENSE) for
the full terms.

This program is distributed in the hope that it will be useful, but **without
any warranty**; without even the implied warranty of merchantability or fitness
for a particular purpose.

### Downloads

eFCrawler retrieves third-party web and PDF resources. Educational intent does
not grant rights to redistribute downloaded material — those rights remain with
the original publishers. Users are responsible for complying with the terms of
any resource they download, and with the terms of service of the research
providers used.

---

## Author

**Dr. Eric O. Flores**
Independent software developer and creator of eFCrawler.

---
## Contributing

Bug reports, feature requests, and questions are welcome.

- **Found a bug?** [Open a bug report](https://github.com/drericflores/efcrawler/issues/new?template=bug_report.yml)
- **Have an idea?** [Request a feature](https://github.com/drericflores/efcrawler/issues/new?template=feature_request.yml)
- **Want to contribute code?** See [CONTRIBUTING.md](CONTRIBUTING.md)

eFCrawler depends on external research providers, so bug reports are most
useful when they include your version, distribution, and which provider was
active. The templates ask for these.

---

## Support

eFCrawler is developed and maintained as an independent project. If you find it
useful and would like to support its continued development, documentation,
testing, and maintenance, voluntary donations are appreciated.

Donations are entirely optional and do not affect access to eFCrawler's
functionality.

**Zelle:** [eoftoro@gmail.com](mailto:eoftoro@gmail.com)

Thank you for supporting the continued development of eFCrawler.
