cmake_minimum_required(VERSION 3.10)

project(TimeSeriesDatabaseEngine C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SOURCES
    src/server.c
)

add_executable(tsdb ${SOURCES})