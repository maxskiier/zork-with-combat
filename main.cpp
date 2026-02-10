#include <any>
#include <iostream>
#include <random>
#include <vector>
#include <string>
#include <ncurses.h>
#include <list>
#include <stdexcept>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <cstring>
#ifndef _WIN32
#include <sys/mman.h>
#endif
#include <cstddef>
#include <zlib.h>
#include <thread>
#include <mutex>
#include <functional>
#include <utility>
#include <unordered_map>
#include <optional>
#include <stop_token>
#include <cassert>
#include "DatSave.hpp"
#include "thrdHandle.hpp"

/*                                                                 *
 *                    COPYRIGHT NOTICE                             *
 *               Copyright 2026 Maxwell Doose                      *
 *                   All rights reserved                           *
 *           Use of this source code without prior                 *
 *    authorization is STRICTLY prohibited by US Copyright Law.    *
 *  When applicable, certain exceptions may be made for Fair Use.  *
 *                                                                 *
*/

#define SECTION_CHKSUM 0x0000 // Offsets for the save function
#define SECTION_CHAR_DAT 0x0004
#define SECTION_INVENTORY_DAT 0x0080
#define SECTION_STATS_MC_DAT 0x0280
#define SECTION_STATS_ENEMIES_DAT 0x0300
#define SECTION_DUNGEON_DAT 0x0B00

typedef std::vector<uint8_t> levVec; // Easier to type, fairly readable
typedef std::string str; // See above comment
typedef unsigned char hex;  // See above comment
typedef std::array<unsigned char, 4> hex4ByteWord;   // Made due to a lack of a 4 byte wide std::byte data type
typedef std::array<unsigned char, 8> hex8ByteWord;  // Made due to a lack of an 8 byte wide std::byte data type
typedef std::array<unsigned char, 10> hex10ByteWord; // Standard 10 byte type for headers, data, etc
typedef std::lock_guard<std::mutex> autoLock;
typedef int windowID;

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

const levVec mc_stats[] // Level stats for main character
{
	{12, 3, 3}, // level 1
	{15, 4, 3}, // level 2
	{17, 5, 4}, // level 3
	{20, 5, 5}, // level 4
	{22, 6, 6} // level 5
};
const levVec basic_enemy_stats[]  // Level stats for basic enemy
{
	{10, 3, 2},
	{13, 3, 2},
	{15, 4, 3},
	{18, 4, 3},
	{19, 4, 4}
};

int width;
int height;
int menuPress;

str gameName = "The Max D. Game";

#ifdef __cplusplus
extern "C" {
#endif
int get_seed()
{
    uint64_t randNum = 0;
    asm volatile ("mrs x0,S3_3_C2_C4_0" : "=r"(randNum));
    /* Obtains number from built in RNG register,
    moves it to general purpose register 1, moves to randNum */
    return randNum;
}
#ifdef __cplusplus
}
#endif

void combat_loop()  // TODO: Implement and optimize
{

}

class stats
{
    private:
        uint8_t health;
        uint8_t attack;
        uint8_t speed;
        uint8_t battle_health;
        uint8_t battle_attack;
        uint8_t battle_speed;
        uint8_t level;
        std::list<str> inventory;
        str char_name;
    public:
        stats(str new_char_name, uint8_t level_offset) // Initialization of an object
        {
            char_name = new_char_name;               // REMINDER: Do not touch!
            level = 1 + level_offset;
            level_up(level);
        }

        int update_and_get_health(int8_t change)
        {
            health += change;
            return health;
        }
        int update_and_get_speed(int8_t change)
        {
            speed += change;
            return speed;
        }
        int update_and_get_attack(int8_t change)
        {
            attack += change;
            return attack;
        }

        int update_and_get_battle_health(int8_t change)
        {
            battle_health += change;
            return battle_health;
        }
        int update_and_get_battle_speed(int8_t change)
        {
            battle_speed += change;
            return battle_speed;
        }
        int update_and_get_battle_attack(int8_t change)
        {
            battle_attack += change;
            return battle_attack;
        }

        void level_up(uint8_t new_lvl)
        {
            health = mc_stats[new_lvl][1];
            attack = mc_stats[new_lvl][2];
            speed = mc_stats[new_lvl][3];
            battle_health = mc_stats[new_lvl][1];
            battle_attack = mc_stats[new_lvl][2];
            battle_speed = mc_stats[new_lvl][3];
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

        void ncThrdIntegrityCheck()
        {
            if (std::this_thread::get_id() != thisThrdId) std::terminate(); else return;
        }

    public:
        std::array<uint8_t,5> keyIO(bool replaceKey=false, uint8_t replaceIndex = 0, uint8_t whichKey = 0)
        {
            autoLock lock(ncCharIOLock);
            if (replaceKey == true) { menuKeys[replaceIndex] = whichKey; }
            return menuKeys;
        }

        void winInit(uint8_t lines = 0, uint8_t cols = 0, uint8_t startY = 0, uint8_t startX = 0) {
            ncThrdIntegrityCheck();
            if (initializedWinFlag) std::terminate();
            initializedWinFlag = true;
            currentWindow = newwin(lines, cols, startY , startX);
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

        ~ncThreadInterface() {
            if (this->initializedWinFlag) { delwin(currentWindow); }
        }
};

class ncSession : public threadHandler
{
    private:
        friend bool ncStopFunc(std::shared_ptr<ncSession> obj);

        std::mutex thrdInitLock;
        std::jthread cursesThread;

        bool initializedFlag = false;

        ncThreadInterface lineRenderer;
    public:
        ~ncSession()
        {
            if (initializedFlag) { endwin(); }
        }

        void cursesInit()
        {
            autoLock lock(thrdInitLock);

            if (initializedFlag) std::terminate();

            thisThrdId = std::this_thread::get_id();
            initializedFlag = true;

            initscr();
            start_color();
            init_pair(1, COLOR_BLACK, COLOR_WHITE);
            {
                lineRenderer.attachThreadId(std::this_thread::get_id());
                lineRenderer.winInit();
                lineRenderer.enableKeyStrobing();

                do
                {
                    lineRenderer.renderLines();
                }
                while (this->thrdFlag != 0b10000000);
            }
            return;
        }
};

bool ncStopFunc(std::shared_ptr<ncSession> obj)
{
    if (obj->cursesThread.joinable())
    {
        obj->cursesThread.join();
        return true;
    } else return false;
}


int main()
{
    threadHandler mainThrd;
    try
    {
        mainThrd.updThrdFlag(thrdAttribBitmask::MAIN);
    } catch(std::invalid_argument e) {

    }

    {
        auto tCurse = std::make_shared<ncSession>();
        tCurse->start([tCurse] {
            tCurse->cursesInit();
        });
        std::this_thread::sleep_for(std::chrono::seconds(15));
        tCurse->updThrdFlag(thrdAttribBitmask::TERM);
        uint8_t i = 0;
        while (true)
        {
            if (ncStopFunc(tCurse) == true) break;
        }
    }
    return 0;
}
