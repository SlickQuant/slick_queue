# SlickQueue

[![CI](https://github.com/SlickQuant/slick_queue/actions/workflows/ci.yml/badge.svg)](https://github.com/SlickQuant/slick_queue/actions/workflows/ci.yml)

SlickQueue is a header-only C++ library that provides a lock-free,
multi-producer multi-consumer (MPMC) queue built on a ring buffer. It is
designed for high throughput concurrent messaging and can optionally operate
over shared memory for inter-process communication.

## Features

- Lock-free operations for multiple producers and consumers
- Header-only implementation
- Optional shared-memory mode
- No dynamic allocation on the hot path

## Getting Started

Simply add the `include` directory to your project's include path and include
the header:

```cpp
#include "slick_queue.h"

slick::SlickQueue<int> queue(1024);
auto slot = queue.reserve();
*queue[slot] = 42;
queue.publish(slot);

uint64_t cursor = 0;
auto result = queue.read(cursor);
```

## Building Tests

A small test suite is provided using CMake and Catch. Build and run the tests
with:

```bash
cmake -S . -B build
cmake --build build --target slick_queue_tests
./build/tests/slick_queue_tests
```

## License

SlickQueue is released under the [MIT License](LICENSE).


