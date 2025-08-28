/********************************************************************************
 * Copyright (c) 2020-2025 SlickTech
 * All rights reserved
 *
 * This file is part of the SlickQueue. Redistribution and use in source and
 * binary forms, with or without modification, are permitted exclusively under
 * the terms of the MIT license which is available at
 * https://github.com/SlickTech/slick_queue/blob/main/LICENSE
 *
 ********************************************************************************/

#pragma once

#include <cstdint>
#include <atomic>
#include <stdexcept>
#include <string>
#include <cassert>

#if defined(_MSC_VER)
#include <windows.h>
#include <tchar.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace slick {

template<typename T>
class SlickQueue {
    struct slot {
        std::atomic_uint_fast64_t data_index{ std::numeric_limits<uint64_t>::max() };
        uint32_t size = 1;
    };

    uint32_t size_;
    uint32_t buffered_size_;
    uint32_t mask_;
    T* data_ = nullptr;
    slot* control_ = nullptr;
    std::atomic_uint_fast64_t* reserved_ = nullptr;
    std::atomic_uint_fast64_t reserved_local_{ 0 };
    bool own_ = false;
    bool use_shm_ = false;
#if defined(_MSC_VER)
    HANDLE hMapFile_ = nullptr;
    LPVOID lpvMem_ = nullptr;
#else
    int shm_fd_ = -1;
    void* lpvMem_ = nullptr;
    std::string shm_name_;
#endif

public:
    SlickQueue(uint32_t size, const char* const shm_name = nullptr)
        : size_(size)
        , buffered_size_(size + 1024)   // add some buffer at the end
        , mask_(size - 1)
        , data_(shm_name ? nullptr : new T[buffered_size_])   
        , control_(shm_name ? nullptr : new slot[buffered_size_])
        , reserved_(shm_name ? nullptr : &reserved_local_)
        , own_(shm_name == nullptr)
        , use_shm_(shm_name != nullptr)
    {
        assert((size && !(size & (size - 1))) && "size must power of 2");
        if (shm_name) {
            allocate_shm_data(shm_name, false);
        }

        // if (own_) {
        //     // invalidate first slot
        //     control_[0].data_index.store(-1, std::memory_order_relaxed);
        // }
    }

    SlickQueue(const char* const shm_name)
        : own_(false)
        , use_shm_(true)
    {
        allocate_shm_data(shm_name, true);
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
#else
        if (lpvMem_) {
            auto BF_SZ = static_cast<size_t>(64 + sizeof(slot) * buffered_size_ + sizeof(T) * buffered_size_);
            munmap(lpvMem_, BF_SZ);
            lpvMem_ = nullptr;
        }
        if (shm_fd_ != -1) {
            close(shm_fd_);
            shm_fd_ = -1;
        }
        if (own_ && !shm_name_.empty()) {
	    shm_unlink(shm_name_.c_str());
	}
#endif

        if (!use_shm_) {
            delete[] data_;
            data_ = nullptr;

            delete[] control_;
            control_ = nullptr;
        }
    }

    bool own_buffer() const noexcept { return own_; }
    bool use_shm() const noexcept { return use_shm_; }
    constexpr uint32_t size() const noexcept { return mask_ + 1; }

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
        if (index != std::numeric_limits<uint64_t>::max() && reserved_->load(std::memory_order_relaxed) < index) {
            // queue has been reset
            read_index = 0;
        }

        if (index == std::numeric_limits<uint64_t>::max() || index < read_index) {
            // data not ready yet
            return std::make_pair(nullptr, 0);
        }

        auto& data = data_[read_index & mask_];
        read_index = slot.data_index + slot.size;
        return std::make_pair(&data, slot.size);
    }

    /**
    * Read the last published data in the queue
    */
    T* read_last() noexcept {
        auto reserved = reserved_->load(std::memory_order_relaxed);
        if (reserved == 0) {
            return nullptr;
        }
        auto index = reserved - 1;

        // find last published data
        auto begin = index & mask_;
        while (control_[index & mask_].data_index.load(std::memory_order_relaxed) != index)
        {
            --index;
            if ((index & mask_) == begin    // looped entire queue 
                || index >= reserved)       // passed 0
            {
                return nullptr;
            }
        }
        return &data_[index & mask_];
    }

    void reset() noexcept {
        if (use_shm_) {
            control_ = new ((uint8_t*)lpvMem_ + 64) slot[buffered_size_];
        } else {
            delete [] control_;
            control_ = new slot[buffered_size_];
        }
        reserved_->store(0, std::memory_order_release);
    }

private:

#if defined(_MSC_VER)
    void allocate_shm_data(const char* const shm_name, bool open_only) {
        DWORD BF_SZ = 64 + sizeof(slot) * buffered_size_ + sizeof(T) * buffered_size_;
        hMapFile_ = NULL;

#ifndef UNICODE
        std::string shmName = shm_name;
#else
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, shm_name, strlen(shm_name), NULL, 0);
        std::wstring shmName(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, shm_name, strlen(shm_name), &shmName[0], size_needed);
#endif

        if (open_only) {
#ifndef UNICODE
            hMapFile_ = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, (LPCSTR)shmName.c_str());
#else
            hMapFile_ = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, (LPCWSTR)shmName.c_str());
#endif
            own_ = false;
            auto err = GetLastError();
            if (hMapFile_ == NULL) {
                throw std::runtime_error("Failed to open shm. err=" + std::to_string(err));
            }

            lpvMem_ = MapViewOfFile(hMapFile_, FILE_MAP_ALL_ACCESS, 0, 0, 64);
            if (!lpvMem_) {
                auto err = GetLastError();
                throw std::runtime_error("Failed to map shm. err=" + std::to_string(err));
            }
            size_ = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(lpvMem_) + sizeof(std::atomic_uint_fast64_t));
            mask_ = size_ - 1;
            buffered_size_ = size_ + 1024;
        }
        else {
            hMapFile_ = CreateFileMapping(
                INVALID_HANDLE_VALUE,               // use paging file
                NULL,                               // default security
                PAGE_READWRITE,                     // read/write access
                0,                                  // maximum object size (high-order DWORD)
                BF_SZ,                              // maximum object size (low-order DWORD)
#ifndef UNICODE
                (LPCSTR)shmName.c_str()             // name of mapping object
#else           
                (LPCWSTR)shmName.c_str()            // name of mapping object
#endif
            );

            own_ = false;
            auto err = GetLastError();
            if (hMapFile_ == NULL) {
		        throw std::runtime_error("Failed to create shm. err=" + std::to_string(err));
            }

            if (err != ERROR_ALREADY_EXISTS) {
                own_ = true;
            }

            lpvMem_ = MapViewOfFile(hMapFile_, FILE_MAP_ALL_ACCESS, 0, 0, BF_SZ);
            if (!lpvMem_) {
                auto err = GetLastError();
                throw std::runtime_error("Failed to map shm. err=" + std::to_string(err));
            }
        }

        if (own_) {
            reserved_ = new (lpvMem_) std::atomic_uint_fast64_t{ 0 };
            *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(lpvMem_) + sizeof(std::atomic_uint_fast64_t)) = mask_ + 1;
            control_ = new ((uint8_t*)lpvMem_ + 64) slot[buffered_size_];
            data_ = new ((uint8_t*)lpvMem_ + 64 + sizeof(slot) * buffered_size_) T[buffered_size_];
        }
        else {
            reserved_ = reinterpret_cast<std::atomic_uint_fast64_t*>(lpvMem_);
            control_ = reinterpret_cast<slot*>((uint8_t*)lpvMem_ + 64);
            data_ = reinterpret_cast<T*>((uint8_t*)lpvMem_ + 64 + sizeof(slot) * buffered_size_);
        }
    }
#else
    void allocate_shm_data(const char* const shm_name, bool open_only) {
        size_t BF_SZ = 64 + sizeof(slot) * buffered_size_ + sizeof(T) * buffered_size_;
        shm_name_ = shm_name;
        int flags = open_only ? O_RDWR : (O_RDWR | O_CREAT | O_EXCL);
        shm_fd_ = shm_open(shm_name, flags, 0666);
        if (shm_fd_ == -1) {
            if (!open_only && errno == EEXIST) {
                // Try opening existing
                shm_fd_ = shm_open(shm_name, O_RDWR, 0666);
                if (shm_fd_ == -1) {
                    throw std::runtime_error("Failed to open existing shm. err=" + std::to_string(errno));
                }
                own_ = false;
            } else {
                throw std::runtime_error("Failed to open/create shm. err=" + std::to_string(errno));
            }
        } else {
            own_ = !open_only;
        }

        if (own_) {
            if (ftruncate(shm_fd_, BF_SZ) == -1) {
                throw std::runtime_error("Failed to size shm. err=" + std::to_string(errno));
            }
        }

        lpvMem_ = mmap(nullptr, BF_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
        if (lpvMem_ == MAP_FAILED) {
            throw std::runtime_error("Failed to map shm. err=" + std::to_string(errno));
        }

        if (own_) {
            reserved_ = new (lpvMem_) std::atomic_uint_fast64_t{ 0 };
            *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(lpvMem_) + sizeof(std::atomic_uint_fast64_t)) = mask_ + 1;
            control_ = new ((uint8_t*)lpvMem_ + 64) slot[buffered_size_];
            data_ = new ((uint8_t*)lpvMem_ + 64 + sizeof(slot) * buffered_size_) T[buffered_size_];
        } else {
            reserved_ = reinterpret_cast<std::atomic_uint_fast64_t*>(lpvMem_);
            control_ = reinterpret_cast<slot*>((uint8_t*)lpvMem_ + 64);
            data_ = reinterpret_cast<T*>((uint8_t*)lpvMem_ + 64 + sizeof(slot) * buffered_size_);
        }
    }
#endif

};

}
