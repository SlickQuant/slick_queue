/********************************************************************************
 * Copyright (c) 2020-0201, SlickTech
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
#include <stdexcept>
#include <string>

#if defined(_MSC_VER)
#include <windows.h>
#include <tchar.h>
#endif

namespace slick {

template<typename T, size_t SIZE>
class SlickQueue {
  static_assert(SIZE && !(SIZE & (SIZE - 1)), "Size must power of 2");

  struct slot {
    std::atomic_uint_fast64_t data_index { 0 };
    uint32_t size = 1;
  };

  static constexpr uint32_t mask_ = SIZE - 1;
  T* data_;
  slot* control_;
  std::atomic_uint_fast64_t* reserved_;
  std::atomic_uint_fast64_t reserved_local_{ 0 };
  bool own_;
  bool use_shm_;
#if defined(_MSC_VER)
  HANDLE hMapFile_ = nullptr;
  LPVOID lpvMem_ = nullptr;
#endif

 public:
  SlickQueue(const char* const shm_name = nullptr, bool open_only = false)
    : data_(shm_name ? nullptr : new T[SIZE + 1024])   // add some buffer at the end
    , control_(shm_name ? nullptr : new slot[SIZE + 1024])
    , reserved_(shm_name ? nullptr : &reserved_local_)
    , own_(shm_name == nullptr)
    , use_shm_(shm_name != nullptr)
  {
    if (shm_name) {
        allocate_shm_data(shm_name, open_only);
    }

    if (own_) {
      // invalidate first slot
      control_[0].data_index.store(1, std::memory_order_relaxed);
    }
  }
  virtual ~SlickQueue() noexcept {
#if defined(_MSC_VER)
    if (lpvMem_) {
      UnmapViewOfFile(lpvMem_);
      lpvMem_ = nullptr;
    }

    if (hMapFile_) {
      printf("Destroy MapFile %p\n", hMapFile_);
      CloseHandle(hMapFile_);
      hMapFile_ = nullptr;
    }
    
    if (!use_shm_) {
        delete [] data_;
        data_ = nullptr;

        delete [] control_;
        control_ = nullptr;
    }
#endif
  }

  bool own_buffer() const noexcept { return own_;  }
  bool use_shm() const noexcept { return use_shm_;  }

  uint64_t initial_reading_index() const noexcept {
      return reserved_->load(std::memory_order_relaxed); 
  }

  uint64_t reserve(uint32_t n = 1) noexcept {
    return reserved_->fetch_add(n, std::memory_order_acq_rel);
  }

  T* operator[] (uint64_t index) noexcept {
    return &data_[index & mask_];
  }

  const T* operator[] (uint64_t index) const noexcept {
    return &data_[index & mask_];
  }

  void publish(uint64_t index, uint32_t n = 1) noexcept {
    auto& slot = control_[index & mask_];
    slot.size = n;
    slot.data_index.store(index, std::memory_order_release);
  }

  std::pair<T*, uint32_t> read(uint64_t& read_index) noexcept {
    auto& slot = control_[read_index & mask_];
    auto index = slot.data_index.load(std::memory_order_relaxed);
    if (reserved_->load(std::memory_order_relaxed) < index) {
      // queue has been reset
      read_index = 0;
    }

    if (index != read_index) {
      // data not ready yet
      return std::make_pair(nullptr, 0);
    }

    auto& data = data_[read_index & mask_];
    read_index += slot.size;
    return std::make_pair(&data, slot.size);
  }

  void reset() noexcept {
    auto next = reserved_->load(std::memory_order_relaxed);
    if (next <= mask_) {
      // data hasn't wrapped yet, need to invalidate
      memset(control_, 0, sizeof(slot)* (SIZE + 1024));
      memset(data_, 0, sizeof(T) * (SIZE + 1024));
    }
    // invalidate first slot
    control_[0].data_index.store(1, std::memory_order_release);
    reserved_->store(0, std::memory_order_release);
  }

 private:

#if defined(_MSC_VER)
  void allocate_shm_data(const char* const shm_name, bool open_only) {
    auto BF_SZ = 64 + sizeof(slot) * (SIZE + 1024) + sizeof(T) * (SIZE + 1024);
    HANDLE hMapFile = NULL;
    if (open_only) {
      hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE,  (LPCWSTR)shm_name);
      own_ = false;
      auto err = GetLastError();
      if (hMapFile == NULL) {
          throw std::runtime_error("Failed to open shm. err=" + std::to_string(err));
      }
    }
    else {
      hMapFile = CreateFileMapping(
          INVALID_HANDLE_VALUE,               // use paging file
          NULL,                               // default security
          PAGE_READWRITE,                     // read/write access
          0,                                  // maximum object size (high-order DWORD)
          BF_SZ,                              // maximum object size (low-order DWORD)
          (LPCWSTR)shm_name                   // name of mapping object
      );

      own_ = false;
      auto err = GetLastError();
      if (hMapFile == NULL) {
          throw std::runtime_error("Failed to create shm. err=" + std::to_string(err));
      }

      if (err != ERROR_ALREADY_EXISTS) {
          own_ = true;
      }

      printf("%s MapFile created %p\n", shm_name, hMapFile);
    }

    void* lpvMem = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, BF_SZ);
    if (!lpvMem) {
      auto err = GetLastError();
      throw std::runtime_error("Failed to map shm. err=" + std::to_string(err));
    }

    if (own_) {
      reserved_ = new (lpvMem) std::atomic_uint_fast64_t{ 0 };
      control_ = new ((uint8_t*)lpvMem + 64) slot[SIZE + 1024];
      data_ = new ((uint8_t*)lpvMem + 64 + sizeof(slot) * (SIZE + 1024)) T[SIZE + 1024];
    }
    else {
      reserved_ = reinterpret_cast<std::atomic_uint_fast64_t*>(lpvMem);
      control_ = reinterpret_cast<slot*>((uint8_t*)lpvMem + 64);
      data_ = reinterpret_cast<T*>((uint8_t*)lpvMem + 64 + sizeof(slot) * (SIZE + 1024));
    }
  }
#else
  void allocateShmData(const char* const shm_name) noexcept {
  }
#endif

};

}
