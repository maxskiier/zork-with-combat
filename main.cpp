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

/* Copyright 2025-2026 Maxwell Doose */

#define SECTION_CHKSUM 0x0000 // Offsets for the save function
#define SECTION_CHAR_DAT 0x0004
#define SECTION_INVENTORY_DAT 0x0080
#define SECTION_STATS_MC_DAT 0x0280
#define SECTION_STATS_ENEMIES_DAT 0x0300
#define SECTION_DUNGEON_DAT 0x0B00

typedef std::vector<uint8_t> levVec; // Easier to type, fairly readable
typedef std::string str; // See above comment
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
        double_t health;
        double_t attack;
        double_t speed;
        double_t battleHealth;
        double_t battleAttack;
        double_t battleSpeed;
        double_t xp;
        std::list<str> inventory;
        str charName;

        template<UnsignedByteOrWord T>
        double_t xpSigmoidAlgo (T var, double x, double L=100, double k = 2, double x0 = 50)
        {
            double_t f_x = L / (1.0 + std::exp(-k * (x - x0)));
            double_t f_0 = L / (1.0 + std::exp(k * x0));
            double_t retVal = f_x - f_0;
            var = retVal;
            return retVal;
        };
    public:
        stats(str newCharName, double_t x, double_t L=100, double_t k = 2, double_t x0 = 50, double_t startingXP = 10) // Currently undergoing reimplementation
        {
            this->charName = newCharName;
            this->xp = startingXP;
            for (uint8_t i = 0; i < 3; i++)
            {
                switch(i)
                {
                    case 0:
                        this->battleHealth = this->xpSigmoidAlgo(this->health, this->xp);
                        break;
                    case 1:
                        this->battleAttack = this->xpSigmoidAlgo(this->attack, this->xp);
                        break;
                    case 2:
                        this->battleSpeed = this->xpSigmoidAlgo(this->speed, this->xp);
                        break;
                }
            }
        }

        uint16_t healthGetterSetter(int8_t change)
        {
            health += change;
            return health;
        }
        uint16_t speedGetterSetter(int8_t change)
        {
            speed += change;
            return speed;
        }
        uint16_t attackGetterSetter(int8_t change)
        {
            attack += change;
            return attack;
        }

        uint16_t battleHealthGetterSetter(int8_t change)
        {
            battleHealth += change;
            return battleHealth;
        }
        uint16_t battleSpeedGetterSetter(int8_t change)
        {
            battleSpeed += change;
            return battleSpeed;
        }
        uint16_t battleAttackGetterSetter(int8_t change)
        {
            battleAttack += change;
            return battleAttack;
        }

        void addXP(uint16_t amntXP)
        {

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

        str buffer = "";
        uint16_t currentChar;

        bool initializedFlag = false;

        std::jthread thisThrdTask;

    public:

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

            thisThrdId = std::this_thread::get_id(); // Lock down thread functions
            initializedFlag = true;

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
                    } catch(std::bad_function_call e) {
                        datSave::createFile("errlog.txt", false);
                        std::terminate(); // This should NEVER happen
                    }
                currentChar = wgetch(lineRenderer.currentWindow);
                const hex procChar = static_cast<hex>(currentChar);
                if (currentChar == KEY_RESIZE) // Compare to constant 410 for SIGRESIZE
                {
                    wprintw(lineRenderer.currentWindow, "%i\n", currentChar);
                    lineRenderer.renderLines(true); // If so, rerender
                    return;
                } else if ((0x00 <= procChar) || (procChar >= 0x7F)) return;
                else { wprintw(lineRenderer.currentWindow, "%i %i\n", currentChar, procChar);
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
        std::stop_token st;
        auto tCurse = std::make_unique<ncSession>();
        tCurse->start();
        std::this_thread::sleep_for(std::chrono::seconds(15));
        tCurse->updThrdFlag(thrdAttribBitmask::TERM);
        uint32_t i = 0;
    }

    return 0;
}
