/********************************************************************************
 * Copyright (c) 2020, SlickTech
 * All rights reserved
 *
 * This file is part of the SlickQueue. Redistribution and use in source and
 * binary forms, with or without modification, are permitted exclusively under
 * the terms of the MIT license which is available at
 * https://github.com/SlickTech/slick_queue/blob/main/LICENSE
 *
 ********************************************************************************/

#include <cstdint>
#include <atomic>

#if defined(_MSC_VER)
#include <windows.h>
#include <tchar.h>
#endif

namespace slick {

template<typename T, size_t SIZE>
class SlickQueue {
  static_assert(SIZE && !(SIZE & (SIZE - 1)), "Size must power of 2");

  struct slot {
    uint32_t n = 1;
    std::atomic_uint_fast64_t id { 0 };
    T data;
  };

 public:
  SlickQueue(const char* const shm_name = nullptr) noexcept
    : data_(shm_name ? nullptr : new slot[SIZE + 1024])   // add some buffer at the end
    , reserved_(shm_name ? nullptr : &reserved_local_)
  {
    if (shm_name) {
      allocateShmData(shm_name);
    }
    // invalidate first slot
    data_[0].id.store(1, std::memory_order_relaxed);
  }
  virtual ~SlickQueue() noexcept {
#if defined(_MSC_VER)
    if (lpvMem_) {
      UnmapViewOfFile(lpvMem_);
      lpvMem_ = nullptr;
    }

    if (hMapFile_) {
      CloseHandle(hMapFile_);
      hMapFile_ = nullptr;
    }
#endif
  }

  uint64_t reserve(uint32_t n = 1) noexcept {
    return reserved_->fetch_add(n, std::memory_order_acq_rel);
  }

  T* operator[] (uint64_t index) noexcept {
    return &data_[index & mask_].data;
  }

  const T* operator[] (uint64_t index) const noexcept {
    return &data_[index & mask_].data;
  }

  void publish(uint64_t index, uint32_t n = 1) noexcept {
    auto& slot = data_[index & mask_];
    slot.n = n;
    slot.id.store(index, std::memory_order_release);
  }

  std::pair<T*, uint32_t> read(uint64_t& read_index) noexcept {
    auto& slot = data_[read_index & mask_];
    auto id = slot.id.load(std::memory_order_relaxed);
    if (reserved_->load(std::memory_order_relaxed) < id) {
      // queue has been reset
      read_index = 0;
    }

    if (id != read_index) {
      // data not ready yet
      return std::make_pair(nullptr, 0);
    }

    read_index += slot.n;
    return std::make_pair(&slot.data, slot.n);
  }

  void reset() noexcept {
    auto next = reserved_->load(std::memory_order_relaxed);
    if (next <= mask_) {
      // data hasn't wrapped yet, need to invalidate
      memset(data_, 0, sizeof(slot) * (SIZE + 1024));
    }
    // invalidate first slot
    data_[0].id.store(1, std::memory_order_release);
    reserved_->store(0, std::memory_order_release);
  }

 private:

#if defined(_MSC_VER)
  void allocateShmData(const char* const shm_name) {
    auto BF_SZ = 64 + sizeof(slot) * (SIZE + 1024);
    HANDLE hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,               // use paging file
        NULL,                               // default security
        PAGE_READWRITE,                     // read/write access
        0,                                  // maximum object size (high-order DWORD)
        BF_SZ,                              // maximum object size (low-order DWORD)
        shm_name                            // name of mapping object
    );

    bool own = false;
    auto err = GetLastError();
    if (hMapFile == NULL) {
        throw std::runtime_error("Failed to create shm. err=" + std::to_string(err));
    }

    if (err != ERROR_ALREADY_EXISTS) {
        own = true;
    }

    void* lpvMem = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, BF_SZ);
    if (!lpvMem) {
        throw std::runtime_error("Failed to map shm. err=" + std::to_string(GetLastError()));
    }

    reserved_ = new (lpvMem) std::atomic_uint_fast64_t{ 0 };
    data_ = new ((uint8_t*)lpvMem + 64) slot[SIZE + 1024];
  }
#else
  void allocateShmData(const char* const shm_name) noexcept {
  }
#endif

 private:
  static constexpr uint32_t mask_ = SIZE - 1;
  slot* data_;
  std::atomic_uint_fast64_t* reserved_;
  std::atomic_uint_fast64_t reserved_local_ {0};
#if defined(_MSC_VER)
  HANDLE hMapFile_ = nullptr;
  LPVOID lpvMem_ = nullptr;
#endif
};

}
