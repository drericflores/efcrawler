# eFCrawler

## Easy & Flexible Crawler

**Autonomous Research and Resource Discovery for Linux**

eFCrawler is a C++23 and Qt6 desktop application designed to simplify online research and resource discovery. It performs structured searches for a topic, collects useful resources, classifies the results, and provides direct access to web pages and downloadable documents such as PDF files.

Rather than requiring the user to manually repeat variations of the same search, eFCrawler automates the discovery process and presents the resulting resources in a single organized interface.

## Current Version

**eFCrawler 0.2.3 — C2R3 Qualified**

Version 0.2.3 C2R3 is the current qualified baseline of eFCrawler.

The qualified release has been tested for application startup, repeated research operations, provider fallback, WEB and PDF discovery, opening resources, direct PDF downloading, Pause/Resume, Stop, free-resource filtering, download-folder access, clean shutdown, restart, and post-restart research.

## Features

- Autonomous multi-query research
- WEB resource discovery
- PDF resource discovery
- Resource type classification
- Multiple research-provider architecture
- Automatic provider fallback
- Direct opening of web resources
- Direct opening of PDF resources
- PDF downloading
- Free-resource filtering
- Pause and Resume research
- Stop active research
- Dedicated eFCrawler download directory
- Dark and light interface themes
- Native Linux desktop application
- C++23 implementation
- Qt6 graphical interface and networking

## Research Providers

eFCrawler uses a provider-based research architecture.

The current implementation includes:

- **DuckDuckGo Provider** — general-purpose web and resource discovery when available.
- **Wikipedia Provider** — fallback research provider with external-resource discovery.

The Provider Manager allows eFCrawler to continue research using another available provider when the primary provider is unavailable.

This architecture is designed to allow additional research providers to be introduced as eFCrawler develops.

## Resource Discovery

eFCrawler distinguishes between ordinary web resources and directly accessible downloadable resources.

Current resource types include:

- WEB
- PDF
- Document
- Audio
- Video

PDF resources can be opened directly or downloaded from within eFCrawler.

## Downloads

Downloaded resources are stored in the eFCrawler directory within the user's standard Linux Downloads location.

Typically:

    ~/Downloads/eFCrawler/

The **Downloads Folder** command provides direct access to this location from the application.

## Platform

eFCrawler is currently developed for Linux systems, particularly Debian/Ubuntu-family distributions such as Pop!_OS.

The application is developed and tested using modern C++ and Qt.

### Technology

- C++23
- Qt6
- CMake
- Qt Widgets
- Qt Network
- Qt SQL

## Build Requirements

A development system requires a C++23-capable compiler, CMake, and the required Qt6 development packages.

On Debian/Ubuntu-derived systems, the required packages can typically be installed with:

    sudo apt install build-essential cmake qt6-base-dev

## Building

Clone the repository:

    git clone git@github.com:drericflores/efcrawler.git
    cd efcrawler

Configure the project:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

Build eFCrawler:

    cmake --build build -j2

Run the application:

    ./build/efcrawler

## Installation

The project includes a CMake installation target for the eFCrawler executable.

After building:

    sudo cmake --install build

Additional Linux desktop integration, including the official eFCrawler application icon and desktop launcher, is being incorporated into the installation system.

## Official Application Icon

The official eFCrawler application icon is:

    resources/efcrawler.png

The icon represents eFCrawler's research and resource-discovery mission.

It is intended for use by the application window, Linux desktop launcher, application menus, and future eFCrawler distribution packages.

## Project Structure

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

## Development Philosophy

eFCrawler is intended to make research easier by reducing repetitive manual searching while keeping discovered resources visible and accessible to the user.

The project emphasizes:

- simple operation
- useful resource discovery
- direct access to discovered material
- resilient research-provider architecture
- native Linux integration
- maintainable modern C++ design

## Project Status

eFCrawler is under active development.

The current stable development baseline is:

**Version 0.2.3 — C2R3 Qualified**

Future development will build from this qualified baseline while preserving previously qualified recovery points.

## Author

**Dr. Eric O. Flores**

Independent software developer and creator of eFCrawler.

## Support eFCrawler

eFCrawler is developed and maintained as an independent software project.

If you find eFCrawler useful and would like to support its continued development, documentation, testing, and maintenance, voluntary donations are appreciated.

Donations are entirely optional and do not affect access to eFCrawler's functionality.

**Zelle:** eoftoro@gmail.com

Thank you for supporting the continued development of eFCrawler.
