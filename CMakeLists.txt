cmake_minimum_required(VERSION 3.10)

project(TimeSeriesDatabaseEngine C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(COMMON_SOURCES
    src/parsor.c
    include/parsor.h
    src/registry.c
    include/registry.h
    src/timestamp.c
    include/timestamp.h
    src/value.c
    include/value.h
    src/chunk.c
    include/chunk.h
    src/bit_io.c
    include/bit_io.h

    src/head.c
    include/head.h
    
    src/flush.c
    include/flush.h

    src/wal.c
    include/wal.h

    src/retention.c
    include/retention.h

    src/downsample.c
    include/downsample.h
)

add_executable(tsdb
    src/main.c
    
    include/uthash.h

    include/server.h
    src/server.c

    src/request.c
    include/request.h

    include/registry.h
    src/registry.c

    include/head.h
    src/head.c

    src/parsor.c
    include/parsor.h
    
    ${COMMON_SOURCES}
)

add_executable(tsdb-cli
    src/client.c
    ${COMMON_SOURCES}
)

add_executable(test_bit_io tests/test_bit_io.c src/bit_io.c)

add_executable(test_timestamp tests/test_timestamp.c src/timestamp.c src/bit_io.c)

add_executable(test_value tests/test_value.c src/value.c src/bit_io.c)
add_executable(tsdb-benchmark benchmark/benchmark.c)
add_executable(test_gorilla tests/test_gorilla.c ${COMMON_SOURCES})
add_executable(test_chunk tests/test_chunk.c src/chunk.c)
add_executable(test_agg tests/test_agg.c src/registry.c src/head.c src/flush.c src/chunk.c src/bit_io.c src/timestamp.c src/value.c src/wal.c src/downsample.c)
add_executable(test_wal tests/test_wal.c src/wal.c src/head.c)
add_executable(test_retention tests/test_retention.c src/retention.c src/registry.c src/head.c src/flush.c src/chunk.c src/bit_io.c src/timestamp.c src/value.c src/wal.c src/downsample.c)
add_executable(test_downsample tests/test_downsample.c src/downsample.c src/registry.c src/head.c src/flush.c src/chunk.c src/bit_io.c src/timestamp.c src/value.c src/wal.c)