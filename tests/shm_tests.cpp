#define CATCH_CONFIG_NO_POSIX_SIGNALS
#include "catch.hh"
#include "../include/slick_queue.h"

using namespace slick;

TEST_CASE("Read empty queue - shm") {
  SlickQueue<int> queue(2);
  uint64_t read_cursor = 0;
  auto read = queue.read(read_cursor);
  REQUIRE(read.first == nullptr);
}

TEST_CASE( "Reserve - shm") {
  SlickQueue<int> queue(2);
  auto reserved = queue.reserve();
  REQUIRE( reserved == 0 );
  REQUIRE( queue.reserve() == 1);
  REQUIRE( queue.reserve() == 2);
}

TEST_CASE( "Read should fail w/o publish - shm") {
  SlickQueue<int> queue(2);
  uint64_t read_cursor = 0;
  auto reserved = queue.reserve();
  auto read = queue.read(read_cursor);
  REQUIRE( read.first == nullptr );
  REQUIRE( read_cursor == 0);
}

TEST_CASE( "Publish and read - shm" ) {
  SlickQueue<int> queue(2);
  uint64_t read_cursor = 0;
  auto reserved = queue.reserve();
  *queue[reserved] = 5;
  queue.publish(reserved);
  auto read = queue.read(read_cursor);
  REQUIRE( read.first != nullptr );
  REQUIRE( read_cursor == 1);
  REQUIRE( *read.first == 5);
}

TEST_CASE( "Publish and read multiple - shm" ) {
  SlickQueue<int> queue(4);
  uint64_t read_cursor = 0;
  auto reserved = queue.reserve();
  *queue[reserved] = 5;
  queue.publish(reserved);
  auto reserved1 = queue.reserve();
  *queue[reserved1] = 12;
  auto reserved2 = queue.reserve();
  *queue[reserved2] = 23;
  queue.publish(reserved2);
  auto read = queue.read(read_cursor);
  REQUIRE( read.first != nullptr );
  REQUIRE( read_cursor == 1);
  REQUIRE( *read.first == 5);

  read = queue.read(read_cursor);
  REQUIRE( read.first == nullptr );
  REQUIRE( read_cursor == 1);

  queue.publish(reserved1);
  read = queue.read(read_cursor);
  REQUIRE(read.first != nullptr);
  REQUIRE(read_cursor == 2);
  REQUIRE(*read.first == 12);

  read = queue.read(read_cursor);
  REQUIRE(read.first != nullptr);
  REQUIRE(read_cursor == 3);
  REQUIRE(*read.first == 23);
}

TEST_CASE("SHM test - shm") {
    SlickQueue<int> queue(2, "test");
    uint64_t read_cursor = 0;
    auto reserved = queue.reserve();
    *queue[reserved] = 5;
    queue.publish(reserved);
    auto read = queue.read(read_cursor);
    REQUIRE(read.first != nullptr);
    REQUIRE(read_cursor == 1);
    REQUIRE(*read.first == 5);
}

