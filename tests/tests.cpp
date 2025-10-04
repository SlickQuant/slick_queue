#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#define CATCH_CONFIG_NO_POSIX_SIGNALS
#include "catch.hh"
#include <slick_queue/slick_queue.h>
#include <thread>

using namespace slick;

TEST_CASE("Read empty queue") {
  SlickQueue<int> queue(2);
  uint64_t read_cursor = 0;
  auto read = queue.read(read_cursor);
  REQUIRE(read.first == nullptr);
}

TEST_CASE( "Reserve") {
  SlickQueue<int> queue(2);
  auto reserved = queue.reserve();
  REQUIRE( reserved == 0 );
  REQUIRE( queue.reserve() == 1);
  REQUIRE( queue.reserve() == 2);
}

TEST_CASE( "Read should fail w/o publish") {
  SlickQueue<int> queue(2);
  uint64_t read_cursor = 0;
  auto reserved = queue.reserve();
  auto read = queue.read(read_cursor);
  REQUIRE( read.first == nullptr );
  REQUIRE( read_cursor == 0);
}

TEST_CASE( "Publish and read" ) {
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

TEST_CASE( "Publish and read multiple" ) {
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

TEST_CASE( "buffer wrap" ) {
  SlickQueue<char> queue(8);
  uint64_t read_cursor = 0;
  
  auto reserved = queue.reserve(3);
  REQUIRE( reserved == 0 );
  memcpy(queue[reserved], "123", 3);
  queue.publish(reserved, 3);
  auto read = queue.read(read_cursor);
  REQUIRE( read.first != nullptr );
  REQUIRE( read_cursor == 3);
  REQUIRE( strncmp(read.first, "123", 3) == 0);

  reserved = queue.reserve(3);
  REQUIRE( reserved == 3 );
  memcpy(queue[reserved], "456", 3);
  queue.publish(reserved, 3);
  read = queue.read(read_cursor);
  REQUIRE( read.first != nullptr );
  REQUIRE( read_cursor == 6);
  REQUIRE( strncmp(read.first, "456", 3) == 0);

  reserved = queue.reserve(3);
  REQUIRE( reserved == 8 );
  memcpy(queue[reserved], "789", 3);

  // read before publish, the read_cursor should changed to new location
  read = queue.read(read_cursor);
  REQUIRE( read_cursor == 8 );
  REQUIRE( read.first == nullptr );
  REQUIRE( read.second == 0 );

  queue.publish(reserved, 3);
  read = queue.read(read_cursor);
  REQUIRE( read.first != nullptr );
  REQUIRE( read_cursor == 11);
  REQUIRE( strncmp(read.first, "789", 3) == 0);
}

TEST_CASE( "Atomic cursor - multiple consumers work-stealing" ) {
  SlickQueue<int> queue(1024);
  std::atomic<uint64_t> shared_cursor{0};
  std::atomic<int> total_consumed{0};

  // Producer: publish 200 items
  std::thread producer([&]() {
    for (int i = 0; i < 200; ++i) {
      auto slot = queue.reserve();
      *queue[slot] = i;
      queue.publish(slot);
    }
  });

  // Multiple consumers sharing atomic cursor
  auto consumer = [&]() {
    int local_count = 0;
    while (total_consumed.load() < 200) {
      auto result = queue.read(shared_cursor);
      if (result.first != nullptr) {
        local_count++;
        total_consumed.fetch_add(1);
      }
    }
    return local_count;
  };

  std::thread c1(consumer);
  std::thread c2(consumer);
  std::thread c3(consumer);

  producer.join();
  c1.join();
  c2.join();
  c3.join();

  // Verify all 200 items were consumed exactly once
  REQUIRE(total_consumed.load() == 200);
  REQUIRE(shared_cursor.load() == 200);
}

