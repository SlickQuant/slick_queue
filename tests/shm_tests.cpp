#define CATCH_CONFIG_NO_POSIX_SIGNALS
#include "catch.hh"
#include <slick_queue/slick_queue.h>
#include <thread>

using namespace slick;

TEST_CASE("Read empty queue - shm") {
  SlickQueue<int> queue(2, "sq_read_empty");
  uint64_t read_cursor = 0;
  auto read = queue.read(read_cursor);
  REQUIRE(read.first == nullptr);
}

TEST_CASE( "Reserve - shm") {
  SlickQueue<int> queue(2, "sq_reserve");
  auto reserved = queue.reserve();
  REQUIRE( reserved == 0 );
  REQUIRE( queue.reserve() == 1);
  REQUIRE( queue.reserve() == 2);
}

TEST_CASE( "Read should fail w/o publish - shm") {
  SlickQueue<int> queue(2, "sq_read_fail");
  uint64_t read_cursor = 0;
  auto reserved = queue.reserve();
  auto read = queue.read(read_cursor);
  REQUIRE( read.first == nullptr );
  REQUIRE( read_cursor == 0);
}

TEST_CASE( "Publish and read - shm" ) {
  SlickQueue<int> queue(2, "sq_publish_read");
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
  SlickQueue<int> queue(4, "sq_publish_read_multiple");
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

TEST_CASE( "Server Client - shm" ) {
  SlickQueue<int> server(4, "sq_server_cleint");
  SlickQueue<int> client("sq_server_cleint");
  REQUIRE(client.size() == 4);

  auto reserved = server.reserve();
  *server[reserved] = 5;
  server.publish(reserved);
  auto reserved1 = server.reserve();
  *server[reserved1] = 12;
  auto reserved2 = server.reserve();
  *server[reserved2] = 23;
  server.publish(reserved2);
  
  uint64_t read_cursor = 0;
  auto read = client.read(read_cursor);
  REQUIRE( read.first != nullptr );
  REQUIRE( read_cursor == 1);
  REQUIRE( *read.first == 5);

  read = client.read(read_cursor);
  REQUIRE( read.first == nullptr );
  REQUIRE( read_cursor == 1);

  server.publish(reserved1);
  read = client.read(read_cursor);
  REQUIRE(read.first != nullptr);
  REQUIRE(read_cursor == 2);
  REQUIRE(*read.first == 12);

  read = client.read(read_cursor);
  REQUIRE(read.first != nullptr);
  REQUIRE(read_cursor == 3);
  REQUIRE(*read.first == 23);
}

TEST_CASE( "Atomic cursor - shared memory work-stealing" ) {
  SlickQueue<int> server(1024, "sq_atomic_cursor_shm");
  SlickQueue<int> client1("sq_atomic_cursor_shm");
  SlickQueue<int> client2("sq_atomic_cursor_shm");

  std::atomic<uint64_t> shared_cursor{0};
  std::atomic<int> total_consumed{0};

  // Server: publish 100 items
  std::thread producer([&]() {
    for (int i = 0; i < 100; ++i) {
      auto slot = server.reserve();
      *server[slot] = i;
      server.publish(slot);
    }
  });

  // Multiple clients sharing atomic cursor via shared memory
  auto consumer = [&](SlickQueue<int>& client) {
    int local_count = 0;
    while (total_consumed.load() < 100) {
      auto result = client.read(shared_cursor);
      if (result.first != nullptr) {
        local_count++;
        total_consumed.fetch_add(1);
      }
    }
    return local_count;
  };

  std::thread c1([&]() { consumer(client1); });
  std::thread c2([&]() { consumer(client2); });

  producer.join();
  c1.join();
  c2.join();

  // Verify all 100 items were consumed exactly once
  REQUIRE(total_consumed.load() == 100);
  REQUIRE(shared_cursor.load() == 100);
}
