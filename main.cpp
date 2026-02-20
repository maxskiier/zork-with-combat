#include <any>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "DatSave.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <mutex>
#include <ncurses.h>
#include <optional>
#include <random>
#include <stdexcept>
#include <stop_token>
#include <string>
#include "thrdHandle.hpp"
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <atomic>

/* Copyright 2025-2026 Maxwell Doose */

#define SECTION_CHKSUM 0x0000 // Offsets for the save function
#define SECTION_CHAR_DAT 0x0004
#define SECTION_INVENTORY_DAT 0x0080
#define SECTION_STATS_MC_DAT 0x0280
#define SECTION_STATS_ENEMIES_DAT 0x0300
#define SECTION_DUNGEON_DAT 0x0B00

typedef std::string str; // Easier to type, readable, etc
typedef std::string string; // See above comment
typedef unsigned char hex;  // See above comment
typedef std::array<unsigned char, 4> hex4ByteWord;   // Made due to a lack of a 4 byte wide std::byte data type
typedef std::array<unsigned char, 8> hex8ByteWord;  // Made due to a lack of an 8 byte wide std::byte data type
typedef std::array<unsigned char, 10> hex10ByteWord; // Standard 10 byte type for headers, data, etc
typedef std::lock_guard<std::mutex> autoLock;
typedef int windowID;

template<typename T>
concept UnsignedByteOrWord = std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, double_t>;

hex8ByteWord saveBuffer;

namespace fsStream {
    using std::ofstream;
    using std::ifstream;
    using std::fstream;
    using std::ios;
}
namespace fsOps = std::filesystem;

class room_attributes
{
    private:
        uint8_t basic_enemies;
        uint8_t advanced_enemies;
        uint8_t bosses;
    public:
        int get_enemy_amount()
        {
            return basic_enemies + advanced_enemies + bosses;
        }
};

uint16_t width;
uint16_t height;
int menuPress;

str gameName = "The Max D. Game";

const std::array<uint8_t,3> mcStats[] // Level stats for main character
{
    {12, 3, 3}, // level 1
    {15, 4, 3}, // level 2
    {17, 5, 4}, // level 3
    {20, 5, 5}, // level 4
    {22, 6, 6} // level 5
};

const std::array<uint8_t,3> basicEnemyStats[]  // Level stats for basic enemy
{
    {10, 3, 2},
    {13, 3, 2},
    {15, 4, 3},
    {18, 4, 3},
    {19, 4, 4}
};

uint64_t getRand()
{
    uint64_t randNum = 0;
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<> distrib(1, 100);
    randNum = distrib(gen);
    return randNum;
}

void combat_loop()  // TODO: Implement and optimize
{

}

class stats
{
private:
    uint8_t health;
    uint8_t attack;
    uint8_t speed;
    uint8_t battleHealth;
    uint8_t battleAttack;
    uint8_t battleSpeed;
    uint8_t level;
    std::list<str> inventory;
    str charName;
public:
    stats(str newCharName, uint8_t levelOffset) // Initialization of an object
    {
        charName = newCharName;               // REMINDER: Do not touch!
        level = 1 + levelOffset;
        levelUp(level);
    }

    int updateAndGetHealth(int8_t change)
    {
        health += change;
        return health;
    }
    int updateAndGetApeed(int8_t change)
    {
        speed += change;
        return speed;
    }
    int updateAndGetAttack(int8_t change)
    {
        attack += change;
        return attack;
    }

    int updateAndGetBattleHealth(int8_t change)
    {
        battleHealth += change;
        return battleHealth;
    }
    int updateAndGetBattleSpeed(int8_t change)
    {
        battleSpeed += change;
        return battleSpeed;
    }
    int updateAndGetBattleAttack(int8_t change)
    {
        battleAttack += change;
        return battleAttack;
    }

    void levelUp(uint8_t new_lvl)
    {
        health = mcStats[new_lvl][1];
        attack = mcStats[new_lvl][2];
        speed = mcStats[new_lvl][3];
        battleHealth = mcStats[new_lvl][1];
        battleAttack = mcStats[new_lvl][2];
        battleSpeed = mcStats[new_lvl][3];
    }
};

class ncThreadInterface // WARNING: Not fully implemented yet
{
    private:
        std::array<uint8_t,5> menuKeys;
        WINDOW *currentWindow;

        std::mutex ncCharIOLock;
        std::mutex thrdInitLock;

        std::thread::id thisThrdId;

        bool initializedWinFlag = false;
        bool initializedThrdFlag = false;

        friend class ncSession;

        void ncThrdIntegrityCheck()
        {
            if (std::this_thread::get_id() != this->thisThrdId) std::terminate(); else return;
        }

    public:
        std::array<uint8_t,5> keyIO(bool replaceKey=false, uint8_t replaceIndex = 0, uint8_t whichKey = 0)
        {
            autoLock lock(ncCharIOLock);
            if (replaceKey == true) { menuKeys[replaceIndex] = whichKey; }
            return menuKeys;
        }

        void winInit(int lines = LINES, int cols = COLS, int startY = 0, int startX = 0) {
            ncThrdIntegrityCheck();
            if (initializedWinFlag) std::terminate();
            initializedWinFlag = true;
            currentWindow = newwin(lines, cols, startY , startX);
            keypad(currentWindow, TRUE);
            return;
        }
        void winRm() {
            ncThrdIntegrityCheck();
            delwin(currentWindow);
            initializedWinFlag = false;
            return;
        }

        void attachThreadId(std::thread::id id) {
            if (this->initializedThrdFlag) std::terminate();
            thisThrdId = id;
        }

        void renderLines(bool disableHeightWidthCheck = false)
        {
            ncThrdIntegrityCheck();
            if (wgetch(currentWindow) != KEY_RESIZE && disableHeightWidthCheck == false)
            {
                return;
            } else {
                width = COLS;
                height = LINES;
                uint8_t len = gameName.length();

                werase(currentWindow);
                wresize(currentWindow, LINES, COLS);
                wattron(currentWindow, COLOR_PAIR(1));
                wmove(currentWindow, 0,0); whline(currentWindow, ' ', COLS);
                mvwaddstr(currentWindow, 0, (COLS-len)/2, gameName.c_str());
                wmove(currentWindow, LINES - 1, 0); whline(currentWindow, ' ', COLS);
                mvwaddstr(currentWindow, LINES-1, 0, "Press Esc for menu");

                wnoutrefresh(currentWindow);
                doupdate();
                attroff(COLOR_PAIR(1));
                return;

            }
        }

        void enableKeyStrobing()
        {
            ncThrdIntegrityCheck();
            nodelay(currentWindow, TRUE);
            return;
        }
        void disableKeyStrobing()
        {
            ncThrdIntegrityCheck();
            nodelay(currentWindow, FALSE);
            return;
        }

        WINDOW* winGetter() { // Deprecated
            ncThrdIntegrityCheck();
            return currentWindow;
        }

        ~ncThreadInterface() {
            if (this->initializedWinFlag) { delwin(currentWindow); }
        }
};

class ncSession : public threadHandler
{
    private:

        ncThreadInterface lineRenderer;

        std::mutex thrdInitLock;


        uint16_t currentChar;

        bool initializedFlag = false;

        str buffer = "\0";

        std::jthread thisThrdTask;

    public:

        ncSession() = default;
        ~ncSession() = default;

        void start() {
            thisThrdTask = std::jthread(
                [this](std::stop_token st) {
                    cursesInit(st);
            });
            return;
        }

        void cursesInit(std::stop_token st)
        {
            if (initializedFlag) std::terminate();

	    buffer.pop_back();

            thisThrdId = std::this_thread::get_id(); // Lock down thread functions
            this->initializedFlag = true;

            initscr();
            start_color();
            init_pair(1, COLOR_BLACK, COLOR_WHITE);

            lineRenderer.attachThreadId(std::this_thread::get_id());
            lineRenderer.winInit();
            lineRenderer.enableKeyStrobing();
            lineRenderer.renderLines(true);

            auto inputBuffer = [this]()
            mutable {
                if (!this->initializedFlag) try {
                        throw std::bad_function_call();
                    } catch(std::bad_function_call  e) {
                        std::terminate(); // This should NEVER happen
                    }
                currentChar = wgetch(lineRenderer.currentWindow);
                const hex procChar = static_cast<hex>(currentChar);
                if (currentChar == KEY_RESIZE) // Compare to constant 410 for SIGRESIZE
                {
                    wprintw(lineRenderer.currentWindow, "%i\n", currentChar);
                    lineRenderer.renderLines(true); // If so, rerender
                    return;
                } else if (!buffer.empty() && procChar == 0x08 || 0x7F || KEY_BACKSPACE)
                    buffer.pop_back();
                else if ((0x20 > procChar) || (procChar <= 0x7F)) return; else
                {
                    mvwprintw(lineRenderer.currentWindow, LINES+1, 0, "> %i %i\n", currentChar, procChar);
                    buffer.append(1, static_cast<char>(procChar));
                    return;
                }
            };

            while (!st.stop_requested())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                inputBuffer();
            }

            endwin();

            return;
        }

    int16_t reportCharGetter()
	{
		return currentChar;
	}
};

int main()
{
    threadHandler mainThrd;
    try
    {
        mainThrd.updThrdFlag(thrdAttribBitmask::MAIN);
    } catch(std::invalid_argument e) {

    }

    {
        auto tCurse = std::make_unique<ncSession>();
        tCurse->start();
        while (tCurse->reportCharGetter() != 0x1B)
	    {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	    }
    }

    return 0;
}
