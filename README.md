# Time Series Database Engine

A high-performance, lightweight time series database engine written in C. Optimized for storing, compressing, and efficiently querying temporal data with support for downsampling, data retention policies, and server-based access.

## Features

- **Efficient Time Series Storage** - Gorilla-inspired compression algorithm for timestamps and values
- **Server Architecture** - TCP/IP based server with client support for distributed queries
- **Data Compression** - Bit-level I/O and delta encoding for minimal storage footprint
- **Downsampling** - Aggregate data at different time intervals (averaging, min, max operations)
- **Data Retention Policies** - Automatic data cleanup based on configurable retention rules
- **Write-Ahead Logging (WAL)** - Ensures data durability and crash recovery
- **Hot Storage** - In-memory head blocks for recent data with automatic flushing to persistent storage
- **Chunk-Based Architecture** - Data organized into time-bound chunks for efficient querying

## Project Structure

```
TimeSeriesDatabaseEngine/
├── include/                    # Header files
│   ├── timestamp.h            # Timestamp encoding/decoding
│   ├── value.h                # Value encoding/decoding
│   ├── bit_io.h               # Bit-level I/O operations
│   ├── chunk.h                # Chunk management
│   ├── head.h                 # In-memory head block
│   ├── registry.h             # Metric registry and storage
│   ├── server.h               # Server and client communication
│   ├── request.h              # Request parsing
│   ├── flush.h                # Flush to persistent storage
│   ├── wal.h                  # Write-ahead logging
│   ├── retention.h            # Data retention policies
│   ├── downsample.h           # Time series downsampling
│   ├── parsor.h               # Query parsing
│   └── uthash.h               # Hash table library (external)
│
├── src/                        # Implementation files
│   ├── timestamp.c            # Timestamp codec implementation
│   ├── value.c                # Value codec implementation
│   ├── bit_io.c               # Bit reader/writer implementation
│   ├── chunk.c                # Chunk operations
│   ├── head.c                 # Head block implementation
│   ├── registry.c             # Metric registry (largest module)
│   ├── server.c               # Server implementation with threading
│   ├── client.c               # Client implementation
│   ├── request.c              # Request handling
│   ├── flush.c                # Flushing mechanism
│   ├── wal.c                  # Write-ahead log
│   ├── retention.c            # Retention policy enforcement
│   ├── downsample.c           # Downsampling algorithms
│   ├── parsor.c               # Query parser
│   └── main.c                 # Entry point
│
├── tests/                      # Test suite
│   ├── test_bit_io.c
│   ├── test_timestamp.c
│   ├── test_value.c
│   ├── test_chunk.c
│   ├── test_gorilla.c
│   ├── test_agg.c
│   ├── test_wal.c
│   ├── test_retention.c
│   └── test_downsample.c
│
├── benchmark/                  # Performance benchmarks
│   └── benchmark.c
│
├── CMakeLists.txt             # Build configuration
├── README.md                  # This file
├── LICENSE                    # MIT License
└── THIRD_PARTY_LICENSES       # Third-party attribution
```

## Core Components

### Registry (`registry.h/c`)
Central data structure maintaining:
- Hash table of all metrics
- Chunk metadata and file references
- In-memory head blocks for recent data

### Head Block (`head.h/c`)
In-memory buffer for recent time series points:
- Stores uncompressed data temporarily
- Automatically flushed to disk periodically
- Optimized for write performance

### Chunk (`chunk.h/c`)
Immutable compressed storage unit:
- Contains multiple time-value pairs
- Uses Gorilla compression
- Stored on disk as binary files

### Compression (`timestamp.h/c`, `value.h/c`, `bit_io.h/c`)
Data compression pipeline:
- **Timestamps**: Delta-of-delta encoding + variable-length integers
- **Values**: XOR compression with leading/trailing zero elimination
- **Bit I/O**: Efficient bit-level read/write operations

### Server (`server.h/c`)
TCP/IP server with:
- Multi-threaded client handling
- Request parsing and execution
- Thread-safe registry access with mutex locks

### Downsampling (`downsample.h/c`)
Time-based aggregation:
- Average, Min, Max operations
- Configurable bucket intervals
- Efficient in-memory aggregation

### Data Retention (`retention.h/c`)
Automatic data cleanup:
- Removes chunks older than retention period
- Customizable per-metric retention policies
- Ensures bounded storage growth

### Write-Ahead Logging (`wal.h/c`)
Durability and recovery:
- Logs all writes before applying to registry
- Recovery on startup from WAL files
- Prevents data loss on crashes

## Building

### Prerequisites
- GCC or Clang compiler
- CMake 3.10 or higher
- POSIX-compliant system (Linux, macOS)

### Build from Source

```bash
git clone https://github.com/talhaismail62/TimeSeriesDatabaseEngine.git
cd TimeSeriesDatabaseEngine
mkdir build
cd build
cmake ..
make
```

### Build Output

```
build/
├── tsdb                    # Time series database server
├── tsdb-cli                # Command-line client
├── test_bit_io             # Bit I/O unit tests
├── test_timestamp          # Timestamp codec tests
├── test_value              # Value codec tests
├── test_chunk              # Chunk tests
├── test_gorilla            # Compression tests
├── test_agg                # Aggregation tests
├── test_wal                # WAL tests
├── test_retention          # Retention policy tests
├── test_downsample         # Downsampling tests
└── tsdb-benchmark          # Performance benchmark
```

## Usage

### Start the Server

```bash
./build/tsdb --port 9999 --datadir /tmp/tsdb_data
```

**Options:**
- `--port` : Server port (default: 9999)
- `--datadir` : Data storage directory

### Connect with Client

```bash
./build/tsdb-cli --host localhost --port 9999
```

### Server Commands

#### INSERT (Write Data)
```
PUT metric_name timestamp value
```
Example:
```
PUT temperature 1687123456 23.5
PUT cpu_usage 1687123457 65.2
```

#### GET (Query Range)
```
GET metric_name start_timestamp end_timestamp
```
Example:
```
GET temperature 1687123000 1687124000
```

#### AGG (Aggregation)
```
AGG metric_name start_timestamp end_timestamp bucket_seconds function
```
Functions: `avg`, `min`, `max`

Example:
```
AGG temperature 1687123000 1687124000 300 avg
```

#### STATS (Metric Statistics)
```
STATS metric_name
```

#### DELETE (Remove Metric)
```
DELETE metric_name
```

## API Overview

### Registry Operations
```c
// Initialize registry at startup
void registry_init(const char *dataDir);

// Write new data point
bool Head_PUT(char *metricName, long timestamp, double value, char *dataDir);

// Read range of data
char *Head_GET(char *metricName, long startTimestamp, long endTimestamp, int *size);

// Aggregated query
char *Head_AGG(char *metricName, long startTimestamp, long endTimestamp, 
               int bucketSeconds, const char *func);

// Get metric statistics
char *Head_STATS(char *metricName);

// Delete metric
void deleteMetric(char *key);

// Flush head to disk
bool headflush(char *metricname, char *dataDir);
```

### Timestamp Encoding
```c
// Initialize encoder
void tsencoderinit(struct timestampencoder *encoder, struct bitwriter *bw);

// Encode timestamp
void tsencoderwrite(struct timestampencoder *encoder, uint64_t timestamp);

// Decode timestamp
void tsdecoderinit(struct timestampdecoder *decoder, struct bitreader *br);
uint64_t tsdecoderread(struct timestampdecoder *decoder);
```

## Running Tests

```bash
cd build

# Run individual tests
./test_bit_io
./test_timestamp
./test_value
./test_chunk
./test_gorilla      # Full compression pipeline
./test_agg          # Aggregation tests
./test_wal          # Write-ahead logging
./test_retention    # Retention policies
./test_downsample   # Downsampling

# Run benchmark
./tsdb-benchmark
```

## Configuration

### Data Retention
Modify retention policies in `retention.h`:
- Default retention period (in seconds)
- Cleanup interval

### Flushing
Configure head block flushing in `head.c`:
- Flush threshold (number of points)
- Flush interval (in seconds)

### Downsampling
Adjust aggregation functions in `downsample.c`:
- Supported functions: average, minimum, maximum
- Custom aggregation functions can be added

## Dependencies

### External Libraries
- **uthash** v2.3+ - Hash table library for C structures
  - Source: https://troydhanson.github.io/uthash/
  - License: BSD 2-Clause
  - Included in: `include/uthash.h`

### System Libraries
- `pthread` - POSIX threads for server concurrency
- `arpa/inet.h` - Network communication (Unix/Linux)

## Performance Characteristics

### Storage Efficiency
- Timestamp compression: ~2-4 bytes per point (delta encoding)
- Value compression: ~1-8 bytes per point (XOR compression)
- **Typical compression ratio**: 10:1 for high-resolution time series

### Query Speed
- Point lookups: O(1) average case (hash table)
- Range queries: O(log n) + O(m) where n = chunks, m = points in range
- Aggregations: Single pass O(m)

### Scalability
- Metrics: Limited only by available memory (hash table)
- Points per metric: Unlimited (chunk-based storage)
- Concurrent connections: Multithreaded per-client handling

## Architecture Decisions

1. **Gorilla Compression** - Time series specific compression achieving 10x reduction
2. **Head Block Pattern** - Separates hot (recent) and cold (archived) data
3. **Chunk-Based Storage** - Immutable chunks enable efficient compression and deletion
4. **Write-Ahead Logging** - Ensures data durability without synchronous disk writes
5. **Hash Table Registry** - O(1) metric lookup for high throughput
6. **Bit-Level I/O** - Maximizes compression for both timestamps and values

## Contributing

Contributions welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the **MIT License** - see the `LICENSE` file for details.

Third-party libraries are licensed under their respective licenses - see `THIRD_PARTY_LICENSES` for details.

## Acknowledgments

- **[Talha Ismail](https://github.com/talhaismail62/)** - Project Lead
- **[Ahmed Nadeem](https://github.com/ahmednadeem18/)** - Developer
- **[Rohail Ashraf](https://github.com/R0HAIL-ASHRAF)** - Developer
- Inspired by [Gorilla Time Series Database](http://www.vldb.org/pvldb/vol8/p1816-teller.pdf) from Facebook
- Uses [uthash](https://troydhanson.github.io/uthash/) by Troy D. Hanson

## Support & Issues

- **Bug Reports**: [GitHub Issues](https://github.com/talhaismail62/TimeSeriesDatabaseEngine/issues)
- **Discussions**: [GitHub Discussions](https://github.com/talhaismail62/TimeSeriesDatabaseEngine/discussions)
- 📧 **Contact**: Open an issue with your question

## Roadmap

- [ ] Persistent index structures for faster range queries
- [ ] Distributed replication support
- [ ] Query optimization and cost-based planning
- [ ] Additional aggregation functions (percentiles, stddev)
- [ ] REST API interface
- [ ] Grafana integration examples
- [ ] Performance profiling and optimization
- [ ] Extended documentation and tutorials

---

**Made with ❤️ for high-performance time series data management**
