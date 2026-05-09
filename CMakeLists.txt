cmake_minimum_required(VERSION 3.10)

project(TimeSeriesDatabaseEngine C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COMMON_SOURCES
    src/parsor.c
    src/parsor.h
    src/bit_io.c
    src/bit_io.h
    src/timestamp.c
    src/timestamp.h
    src/value.c
    src/value.h
    src/chunk.c
    src/chunk.h
    src/flush.c
    src/flush.h
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

add_executable(test_bit_io tests/test_bit_io.c src/bit_io.c)

add_executable(test_timestamp tests/test_timestamp.c src/timestamp.c src/bit_io.c)

add_executable(test_value tests/test_value.c src/value.c src/bit_io.c)

add_executable(test_chunk tests/test_chunk.c src/chunk.c)
