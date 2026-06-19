cmake_minimum_required(VERSION 3.16)
project(glowcontrol LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS DBus Widgets)
find_package(KF6StatusNotifierItem REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(DDCUTIL REQUIRED IMPORTED_TARGET ddcutil)

add_executable(glowcontrol
    include/glowcontrol/controlpanel.h
    src/controlpanel.cpp
    include/glowcontrol/ddc.h
    src/ddc.cpp
    src/main.cpp
    include/glowcontrol/osdwindow.h
    src/osdwindow.cpp
    include/glowcontrol/nativeosd.h
    src/nativeosd.cpp
    include/glowcontrol/shortcut.h
    src/shortcut.cpp
    include/glowcontrol/trayapp.h
    src/trayapp.cpp
    resources.qrc
)

target_include_directories(glowcontrol PRIVATE include)

target_link_libraries(glowcontrol PRIVATE
    Qt6::DBus
    Qt6::Widgets
    KF6::StatusNotifierItem
    PkgConfig::DDCUTIL
)
