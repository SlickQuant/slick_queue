# SlickQueue

[![CI](https://github.com/SlickQuant/slick_queue/actions/workflows/ci.yml/badge.svg)](https://github.com/SlickQuant/slick_queue/actions/workflows/ci.yml)

SlickQueue is a header-only C++ library that provides a lock-free,
multi-producer multi-consumer (MPMC) queue built on a ring buffer. It is
designed for high throughput concurrent messaging and can optionally operate
over shared memory for inter-process communication.

## Features

- **Lock-free operations** for multiple producers and consumers
- **Header-only implementation** - just include and go
- **Zero dynamic allocation** on the hot path for predictable performance
- **Shared memory support** for inter-process communication
- **Cross-platform** - supports Windows, Linux, and macOS
- **Modern C++20** implementation

## Requirements

- C++20 compatible compiler
- CMake 3.10+ (for building tests)

## Installation

SlickQueue is header-only. Simply add the `include` directory to your project's include path:

```cpp
#include "slick_queue/slick_queue.h"
```

### Using CMake

```cmake
include(FetchContent)

# Disable tests for slick_queue
set(BUILD_SLICK_QUEUE_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    slick_queue
    GIT_REPOSITORY https://github.com/SlickQuant/slick_queue.git
    GIT_TAG v1.1.0.0  # See https://github.com/SlickQuant/slick_queue/releases for latest version
)
FetchContent_MakeAvailable(slick_queue)

target_link_libraries(your_target PRIVATE slick_queue)
```

## Usage

### Basic Example

```cpp
#include "slick_queue/slick_queue.h"

// Create a queue with 1024 slots (must be power of 2)
slick::SlickQueue<int> queue(1024);

// Producer: reserve a slot, write data, and publish
auto slot = queue.reserve();
*queue[slot] = 42;
queue.publish(slot);

// Consumer: read from queue using a cursor
uint64_t cursor = 0;
auto result = queue.read(cursor);
if (result.first != nullptr) {
    int value = *result.first;  // value == 42
}
```

### Shared Memory Example (IPC)

```cpp
#include "slick_queue/slick_queue.h"

// Process 1 (Server/Writer)
slick::SlickQueue<int> server(1024, "my_queue");
auto slot = server.reserve();
*server[slot] = 100;
server.publish(slot);

// Process 2 (Client/Reader)
slick::SlickQueue<int> client("my_queue");
uint64_t cursor = 0;
auto result = client.read(cursor);
if (result.first != nullptr) {
    int value = *result.first;  // value == 100
}
```

### Multi-Producer Multi-Consumer

```cpp
#include "slick_queue/slick_queue.h"
#include <thread>

slick::SlickQueue<int> queue(1024);

// Multiple producers
auto producer = [&](int id) {
    for (int i = 0; i < 100; ++i) {
        auto slot = queue.reserve();
        *queue[slot] = id * 1000 + i;
        queue.publish(slot);
    }
};

// Multiple consumers (each maintains independent cursor)
// Note: Each consumer will see ALL published items (broadcast pattern)
auto consumer = [&](int id) {
    uint64_t cursor = 0;
    int count = 0;
    while (count < 200) {  // 2 producers × 100 items each
        auto result = queue.read(cursor);
        if (result.first != nullptr) {
            int value = *result.first;
            ++count;
        }
    }
};

std::thread p1(producer, 1);
std::thread p2(producer, 2);
std::thread c1(consumer, 1);
std::thread c2(consumer, 2);

p1.join(); p2.join();
c1.join(); c2.join();
```

### Work-Stealing with Shared Atomic Cursor

```cpp
#include "slick_queue/slick_queue.h"
#include <thread>
#include <atomic>

slick::SlickQueue<int> queue(1024);
std::atomic<uint64_t> shared_cursor{0};

// Multiple producers
auto producer = [&](int id) {
    for (int i = 0; i < 100; ++i) {
        auto slot = queue.reserve();
        *queue[slot] = id * 1000 + i;
        queue.publish(slot);
    }
};

// Multiple consumers sharing atomic cursor (work-stealing/load-balancing)
// Each item is consumed by exactly ONE consumer
auto consumer = [&]() {
    int count = 0;
    for (int i = 0; i < 100; ++i) {
        auto result = queue.read(shared_cursor);
        if (result.first != nullptr) {
            int value = *result.first;
            ++count;
        }
    }
    return count;
};

std::thread p1(producer, 1);
std::thread p2(producer, 2);
std::thread c1(consumer);
std::thread c2(consumer);

p1.join(); p2.join();
c1.join(); c2.join();
// Total items consumed: 200 (each item consumed exactly once)
```

## API Overview

### Constructor

```cpp
// In-process queue
SlickQueue(uint32_t size);

// Shared memory queue
SlickQueue(uint32_t size, const char* shm_name);  // Writer/Creator
SlickQueue(const char* shm_name);                  // Reader/Attacher
```

### Core Methods

- `uint64_t reserve()` - Reserve a slot for writing (blocks if queue is full)
- `T* operator[](uint64_t slot)` - Access reserved slot
- `void publish(uint64_t slot)` - Publish written data to consumers
- `std::pair<T*, uint32_t> read(uint64_t& cursor)` - Read next available item (independent cursor)
- `std::pair<T*, uint32_t> read(std::atomic<uint64_t>& cursor)` - Read next available item (shared atomic cursor for work-stealing)
- `uint32_t size()` - Get queue capacity

## Performance Characteristics

- **Lock-free**: No mutex contention between producers/consumers
- **Wait-free reads**: Consumers never block each other
- **Cache-friendly**: Ring buffer design with power-of-2 sizing
- **Predictable**: No allocations or system calls on hot path (except for initial reserve when full)

## Building and Testing

### Build Tests

```bash
cmake -S . -B build
cmake --build build
```

### Run Tests

```bash
# Using CTest
cd build
ctest --output-on-failure

# Or run directly
./build/tests/slick_queue_tests
```

### Build Options

- `BUILD_SLICK_QUEUE_TESTS` - Enable/disable test building (default: ON)
- `CMAKE_BUILD_TYPE` - Set to `Release` or `Debug`

## License

SlickQueue is released under the [MIT License](LICENSE).


