cmake_minimum_required(VERSION 3.10)
project(WeatherPort LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)

add_executable(
    logger
    main.cpp
    libs/SerialPort.cpp
)

target_include_directories(logger PRIVATE libs)

target_include_directories(logger PRIVATE ${SQLite3_INCLUDE_DIRS})

target_link_libraries(logger PRIVATE SQLite::SQLite3)

if(UNIX)
    if(APPLE)
        execute_process(
            COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE MACOS_SDK_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(MACOS_SDK_PATH)
            target_include_directories(logger PRIVATE "${MACOS_SDK_PATH}/usr/include")
        endif()
    endif()

    target_link_libraries(logger PRIVATE pthread)
endif()

if(WIN32)
    target_link_libraries(logger PRIVATE ws2_32)
    target_compile_definitions(logger PRIVATE _WIN32_WINNT=0x0601)

    find_package(Threads REQUIRED)
    target_link_libraries(logger PRIVATE Threads::Threads)
endif()
