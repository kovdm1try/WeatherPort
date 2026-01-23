cmake_minimum_required(VERSION 3.16)
project(WeatherGUI VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(USE_QTCHARTS "Use QtCharts instead of Qwt" OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

set(QWT_FOUND FALSE)
set(QWT_INCLUDE_DIRS "")
set(QWT_LIBRARIES "")

if(NOT USE_QTCHARTS)
    if(APPLE)
        if(EXISTS "/opt/homebrew/opt/qwt-qt6/lib/qwt.framework")
            set(QWT_INCLUDE_DIRS "/opt/homebrew/opt/qwt-qt6/lib/qwt.framework/Headers")
            set(QWT_LIBRARIES "/opt/homebrew/opt/qwt-qt6/lib/qwt.framework")
            set(QWT_FOUND TRUE)
        elseif(EXISTS "/opt/homebrew/opt/qwt/lib/qwt.framework")
            set(QWT_INCLUDE_DIRS "/opt/homebrew/opt/qwt/lib/qwt.framework/Headers")
            set(QWT_LIBRARIES "/opt/homebrew/opt/qwt/lib/qwt.framework")
            set(QWT_FOUND TRUE)
        elseif(EXISTS "/usr/local/opt/qwt/lib/qwt.framework")
            set(QWT_INCLUDE_DIRS "/usr/local/opt/qwt/lib/qwt.framework/Headers")
            set(QWT_LIBRARIES "/usr/local/opt/qwt/lib/qwt.framework")
            set(QWT_FOUND TRUE)
        endif()
    endif()

    if(UNIX AND NOT APPLE)
        find_path(QWT_INCLUDE_DIRS qwt.h
            PATHS /usr/include/qwt /usr/local/include/qwt /usr/include/qwt6 /usr/local/include/qwt6
        )
        find_library(QWT_LIBRARIES NAMES qwt qwt6 qwt-qt5 qwt-qt6
            PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
        )
        if(QWT_INCLUDE_DIRS AND QWT_LIBRARIES)
            set(QWT_FOUND TRUE)
        endif()
    endif()

    if(WIN32)
        find_path(QWT_INCLUDE_DIRS qwt.h
            PATHS "C:/Qwt/include" "$ENV{QWT_ROOT}/include"
        )
        find_library(QWT_LIBRARIES NAMES qwt qwt6
            PATHS "C:/Qwt/lib" "$ENV{QWT_ROOT}/lib"
        )
        if(QWT_INCLUDE_DIRS AND QWT_LIBRARIES)
            set(QWT_FOUND TRUE)
        endif()
    endif()
endif()

if(QWT_FOUND AND NOT USE_QTCHARTS)
    message(STATUS "Using Qwt for plotting")
    message(STATUS "Qwt include: ${QWT_INCLUDE_DIRS}")
    message(STATUS "Qwt library: ${QWT_LIBRARIES}")

    set(SOURCES
        main.cpp
        mainwindow.cpp
        temperaturechart.cpp
        apiclient.cpp
    )

    set(HEADERS
        mainwindow.h
        temperaturechart.h
        apiclient.h
    )

    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${QWT_INCLUDE_DIRS}
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        ${QWT_LIBRARIES}
    )

else()
    message(STATUS "Qwt not found or USE_QTCHARTS=ON, using QtCharts")
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Charts)

    set(SOURCES
        main_qtcharts.cpp
        mainwindow_qtcharts.cpp
        apiclient.cpp
    )

    set(HEADERS
        mainwindow_qtcharts.h
        apiclient.h
    )

    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Charts
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_QTCHARTS)
endif()

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER com.weatherport.gui
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
endif()

message(STATUS "Qt version: ${QT_VERSION_MAJOR}.${Qt${QT_VERSION_MAJOR}_VERSION}")
