cmake_minimum_required(VERSION 3.10)

project(TimeSeriesDatabaseEngine C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COMMON_SOURCES
    src/parsor.c
    src/parsor.h
)

add_executable(tsdb
    src/main.c
    
    src/uthash.h

    src/server.h
    src/server.c

    src/request.c
    src/request.h

    src/registry.h
    src/registry.c

    src/head.h
    src/head.c

    src/parsor.c
    src/parsor.h
    
    ${COMMON_SOURCES}
)

add_executable(tsdb-cli
    src/client.c
    ${COMMON_SOURCES}
)