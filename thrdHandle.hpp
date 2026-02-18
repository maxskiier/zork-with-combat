#pragma once

#ifndef THRDHANDLE_HPP
#define THRDHANDLE_HPP


#include <iostream>
#include <ncurses.h>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <thread>
#include <mutex>
#include <functional>
#include <utility>
#include <optional>
#include <stop_token>
#include <cassert>

/* Copyright 2025-2026 Maxwell Doose */

enum class thrdAttribBitmask : uint8_t { MAIN, LOGIC, NCURSES, SAVER, MUTEX, SIGHANDLER, PLACEHOLDER, TERM };

typedef std::vector<uint8_t> levVec; // Easier to type, fairly readable
typedef std::string str; // See above comment
typedef unsigned char hex;  // See above comment
typedef std::array<unsigned char, 4> hex4ByteWord;   // Made due to a lack of a 4 byte wide std::byte data type
typedef std::array<unsigned char, 8> hex8ByteWord;  // Made due to a lack of an 8 byte wide std::byte data type
typedef std::array<unsigned char, 10> hex10ByteWord; // Standard 10 byte type for headers, data, etc
typedef std::lock_guard<std::mutex> autoLock;
typedef int windowID;

class  threadHandler
{
protected:

    bool initializedFlag = false;

    uint8_t thrdFlag = 0b00000000;
    /*
         * thrdFlag Implementation: shows the thread type
         * Bit one denotes the main thread
         * Bit two denotes a logic thread
         * Bit three denotes the thread running ncurses
         * Bit four denotes the save thread
         * Bit five denotes if this thread requires mutual exclusion
         * Bit six denotes the signal handler thread
         * Bit eight denotes when this thread should be terminated; enables safe termination, thread will see it
         */
    std::optional<std::thread::id> thisThrdId;
    std::jthread thisThrdTask;
    std::mutex destructorLock;
    std::mutex updBitLock;
    std::mutex thrdStateLock;

public:

    ~threadHandler() = default;

    template<typename func, typename ...args>
    [[deprecated("Use specific thread starters for each class")]]void start(func&& function, args&&... arguments)
    /* Destructor f**king hates it,
    almost always segfaults, so in short, DO NOT USE. */
    {
        thisThrdTask = std::jthread(
            std::forward<func>(function),
            std::forward<args>(arguments)...
            );
        initializedFlag = true;
    }

    uint8_t getThrdFlag(thrdAttribBitmask mask)
    {
        switch (mask)
        {
        case thrdAttribBitmask::MAIN:
            return thrdFlag & 0b00000001;
        case thrdAttribBitmask::LOGIC:
            return thrdFlag & 0b00000010;
        case thrdAttribBitmask::NCURSES:
            return thrdFlag & 0b00000100;
        case thrdAttribBitmask::SAVER:
            return thrdFlag & 0b00001000;
        case thrdAttribBitmask::MUTEX:
            return thrdFlag & 0b00010000;
        case thrdAttribBitmask::SIGHANDLER:
            return thrdFlag & 0b00100000;
        default:
            throw std::invalid_argument("Corrupted thrdAttribBitmask");
            return 0;
        }
    }
    void updThrdFlag(thrdAttribBitmask mask)
    {
        autoLock lock(updBitLock);
        uint8_t mutexStatus = thrdFlag & 0b00010000;

        switch (mask)
        {
        case thrdAttribBitmask::MAIN:
            thrdFlag = 0b00000001;
            break;
        case thrdAttribBitmask::LOGIC:
            thrdFlag = 0b00000010;
            break;
        case thrdAttribBitmask::NCURSES:
            thrdFlag = 0b00000100;
            break;
        case thrdAttribBitmask::SAVER:
            thrdFlag = 0b00001000;
            break;
        case thrdAttribBitmask::MUTEX:
            thrdFlag ^= 0b00010000;
            break;
        case thrdAttribBitmask::SIGHANDLER:
            thrdFlag = 0b00100000;
            break;
        case thrdAttribBitmask::TERM:
            thrdFlag = 0b10000000;
            break;
        default:
            throw std::invalid_argument("Corrupted bitmask");
        }
        thrdFlag = thrdFlag | mutexStatus;

        return;
    }
};
#endif // THRDHANDLE_HPP
