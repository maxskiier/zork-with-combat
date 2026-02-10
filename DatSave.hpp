#pragma once

#ifndef DATSAVE_HPP
#define DATSAVE_HPP
#endif // DATSAVE_HPP

#define SECTION_HEADER_ID

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <ncurses.h>

/*                                                                 *
 *                    COPYRIGHT NOTICE                             *
 *               Copyright 2026 Maxwell Doose                      *
 *                   All rights reserved                           *
 *           Use of this source code without prior                 *
 *    authorization is STRICTLY prohibited by US Copyright Law.    *
 *  When applicable, certain exceptions may be made for Fair Use.  *
 *                                                                 *
*/

namespace fsStream {
    using std::ofstream;
    using std::ifstream;
    using std::fstream;
    using std::ios;
}
namespace fsOps = std::filesystem;

typedef unsigned char hex;
typedef std::array<unsigned char, 4> hex4ByteWord;
typedef std::array<unsigned char, 8> hex8ByteWord;
typedef std::string str;
typedef std::array<unsigned char, 10> hex10ByteWord;

class DatSave
{
    private:
        inline static str saveFile;
        inline static hex8ByteWord currentDataPage;
        inline static hex10ByteWord currentHeader;
        inline static std::error_code ec;
        inline static constexpr std::size_t pageSize = 8;
        inline static constexpr std::size_t headerSize = 10;
        inline static fsOps::path savePath;
    public:
        static int createFile(const char* fileName = "zkcmt.dat")
        {
            fsStream::ifstream inFile(fileName);
            if (fsOps::exists(fileName))
            {
                return 1;
            }
            inFile.close();
            saveFile = fileName;
            try
            {
                fsOps::create_directories("./Saves");
                savePath = fsOps::path("Saves") / saveFile;
                {
                    std::ofstream out(savePath, std::ios::binary);
                }
                fsOps::resize_file(savePath, 8192, ec);
            } catch(const fsOps::filesystem_error* e) {
                attron(COLOR_PAIR(1));
                printw("Filesystem error: ");
                attroff(COLOR_PAIR(1));
                printw("%s\nPress any key to continue.", e);
                refresh();
                getch();
                return 2;
            }
            return 0;
        }

        static hex8ByteWord dataParser(int sectionOffset)
        {
            fsStream::ifstream datFile(saveFile, fsStream::ios::in | fsStream::ios::binary);
            if (!datFile.is_open()) // Make sure the file handle opened correctly, if it wasn't then we throw an exception
            {
                throw fsOps::filesystem_error("File handle did not open or quit unexpectedly", ec);
            }
            // mmap(NULL, 8192, PROT_READ, MAP_PRIVATE, saveFile, 0); ** reserved for future use
            datFile.read(reinterpret_cast<char*>(currentDataPage.data()), pageSize);
            return currentDataPage;
        }
        static hex getAndIndexPage(int areaOffset)
        {
            return currentDataPage[areaOffset];
        }
        static void singleByteWrite(hex stagedValue, int areaOffset)
        {
            currentDataPage[areaOffset] = stagedValue;
        }

        static hex10ByteWord headerParser(int sectionOffset)
        {
            fsStream::ifstream datFile(saveFile, fsStream::ios::in | fsStream::ios::binary);
            if (!datFile.is_open()) // Make sure the file handle opened correctly, if it wasn't then we throw an exception
            {
                throw fsOps::filesystem_error("File handle did not open or quit unexpectedly", ec);
            }
            datFile.seekg(sectionOffset, fsStream::ios::beg);
            datFile.read(reinterpret_cast<char*>(currentHeader.data()), headerSize);
            return currentHeader;
        }
        static hex getAndIndexHeader(int areaOffset)
        {
            return currentHeader[areaOffset];
        }

        static std::error_code accessErrorCode() {
            return ec;
        }
};
