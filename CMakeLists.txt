cmake_minimum_required(VERSION 3.28)

project(eFCrawler
    VERSION 0.2.3
    DESCRIPTION
        "Easy Flexible Autonomous Research and Resource Discovery"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core
    Widgets
    Network
)

qt_standard_project_setup()

qt_add_executable(efcrawler
    src/main.cpp

    resources/efcrawler.qrc

    src/core/ResearchEngine.cpp
    src/core/ResearchEngine.hpp
    src/model/ResourceType.hpp
    src/model/ResourceClassifier.hpp
    src/model/ResourceClassifier.cpp
    src/model/MediaProbe.hpp
    src/model/MediaProbe.cpp
    src/core/LicenseGate.hpp
    src/core/LicenseGate.cpp
    src/providers/InternetArchiveProvider.hpp
    src/providers/InternetArchiveProvider.cpp
    src/providers/CommonsProvider.hpp
    src/providers/CommonsProvider.cpp
    src/model/SearchResult.hpp

    src/providers/SearchProvider.hpp
    src/providers/DuckDuckGoProvider.cpp
    src/providers/DuckDuckGoProvider.hpp
    src/providers/ProviderManager.cpp
    src/providers/ProviderManager.hpp
    src/providers/WikipediaProvider.cpp
    src/providers/WikipediaProvider.hpp
)

target_compile_definitions(efcrawler PRIVATE
    EFCRAWLER_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(efcrawler PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

target_compile_options(efcrawler PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

install(TARGETS efcrawler
    RUNTIME DESTINATION bin
)
