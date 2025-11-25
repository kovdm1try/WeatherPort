cmake_minimum_required(VERSION 3.10)
project(WeatherPort LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
        logger
        main.cpp
        libs/SerialPort.cpp
)

target_include_directories(logger PRIVATE libs)
