cmake_minimum_required(VERSION 3.20)
project(autoclicker VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets DBus LinguistTools)
find_package(LayerShellQt REQUIRED)

add_executable(autoclicker
    src/main.cpp
    src/MainWindow.cpp
    src/TargetPicker.cpp
    src/VirtualPointer.cpp
    src/ClickWorker.cpp
    src/MouseWatcher.cpp
    src/KWinScript.cpp
)

target_compile_options(autoclicker PRIVATE -Wall -Wextra)
# LayerShellQt::Window::setScreen() is only available from 6.4 on
if(LayerShellQt_VERSION VERSION_GREATER_EQUAL 6.4)
    target_compile_definitions(autoclicker PRIVATE LAYERSHELLQT_HAS_SETSCREEN)
endif()
target_link_libraries(autoclicker PRIVATE
    Qt6::Widgets
    Qt6::DBus
    LayerShellQt::Interface
)

# UI translations, embedded as resources under :/i18n.
# English is the source language; update the .ts files with:
#   cmake --build build --target update_translations
qt_add_translations(autoclicker
    TS_FILES i18n/autoclicker_de.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete -locations none
)

include(GNUInstallDirs)
install(TARGETS autoclicker RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/autoclicker.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/autoclicker.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
