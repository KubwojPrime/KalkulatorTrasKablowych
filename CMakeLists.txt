cmake_minimum_required(VERSION 3.24)

project(KalkulatorTrasKablowych
    VERSION 0.4.0
    DESCRIPTION "Kalkulator wypelnienia, masy i obciazenia ogniowego tras kablowych"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(KTK_BUILD_TESTS "Build calculation tests" ON)
option(KTK_LICENSE_REQUIRED "Require remote authorization at application startup" OFF)
set(KTK_LICENSE_ENDPOINT "" CACHE STRING "HTTPS endpoint used to verify application access")
set(KTK_OFFLINE_GRACE_HOURS "24" CACHE STRING "Offline grace period after successful authorization")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Sql Network)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/QXlsx/QXlsx/CMakeLists.txt")
    message(FATAL_ERROR "QXlsx submodule is missing. Run: git submodule update --init --recursive")
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/QXlsx/QXlsx EXCLUDE_FROM_ALL)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/config.h"
    @ONLY
)

add_library(ktk_core STATIC
    src/domain/calculator.cpp
    src/domain/calculator.h
    src/domain/catalogfilter.cpp
    src/domain/catalogfilter.h
    src/domain/fireloadestimator.cpp
    src/domain/fireloadestimator.h
    src/domain/types.h
)
target_link_libraries(ktk_core PUBLIC Qt6::Core)
target_include_directories(ktk_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(KalkulatorTrasKablowych WIN32
    src/main.cpp
    src/data/catalogrepository.cpp
    src/data/catalogrepository.h
    src/io/xlsxprojectio.cpp
    src/io/xlsxprojectio.h
    src/licensing/accesscontroller.cpp
    src/licensing/accesscontroller.h
    src/ui/cabletablemodel.cpp
    src/ui/cabletablemodel.h
    src/ui/catalogdialog.cpp
    src/ui/catalogdialog.h
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/routevisualizationwidget.cpp
    src/ui/routevisualizationwidget.h
    src/ui/theme.cpp
    src/ui/theme.h
)

qt_add_resources(KalkulatorTrasKablowych "ktk_resources"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        resources/catalog-seed.json
)

target_include_directories(KalkulatorTrasKablowych PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_link_libraries(KalkulatorTrasKablowych PRIVATE
    ktk_core
    QXlsx::QXlsx
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
)

if(MINGW)
    target_compile_options(KalkulatorTrasKablowych PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(ktk_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS KalkulatorTrasKablowych RUNTIME DESTINATION .)
install(FILES
    LICENSE.md
    README.md
    THIRD_PARTY_NOTICES.md
    DESTINATION .
)
install(FILES
    docs/fire-load-estimation.md
    DESTINATION docs
)
install(FILES
    third_party/QXlsx/LICENSE
    DESTINATION licenses
    RENAME QXlsx-MIT.txt
)

include(CTest)
if(BUILD_TESTING AND KTK_BUILD_TESTS)
    add_executable(ktk_tests
        tests/ktk_tests.cpp
        src/data/catalogrepository.cpp
        src/data/catalogrepository.h
        src/io/xlsxprojectio.cpp
        src/io/xlsxprojectio.h
    )
    qt_add_resources(ktk_tests "ktk_test_resources"
        PREFIX "/"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        FILES
            resources/catalog-seed.json
    )
    target_include_directories(ktk_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(ktk_tests PRIVATE
        ktk_core
        QXlsx::QXlsx
        Qt6::Core
        Qt6::Gui
        Qt6::Sql
    )
    add_test(NAME core_and_xlsx COMMAND ktk_tests)
endif()
