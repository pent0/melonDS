/*
    Copyright 2016-2017 StapleButter

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "NDSCart.h"
#include "ARM.h"

#include "melon_fopen.h"

#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
using namespace Windows::Storage;
#endif

namespace NDSCart_SRAM
{

// BIG SRAM TODO!!!!
// USE A DATABASE TO IDENTIFY SAVE MEMORY TYPE
// (and maybe keep autodetect as a last resort??)
// AUTODETECT IS BLARGY AND IS A MESS

u8* SRAM;
u32 SRAMLength;

char SRAMPath[1024];

void (*WriteFunc)(u8 val, bool islast);

u32 Discover_MemoryType;
u32 Discover_Likeliness;
u8* Discover_Buffer;
u32 Discover_DataPos;
u32 Discover_LastPC;
u32 Discover_AddrLength;
u32 Discover_Addr;
u32 Discover_LikelySize;

u32 Hold;
u8 CurCmd;
u32 DataPos;
u8 Data;

u8 StatusReg;
u32 Addr;


void Write_Null(u8 val, bool islast);
void Write_EEPROMTiny(u8 val, bool islast);
void Write_EEPROM(u8 val, bool islast);
void Write_Flash(u8 val, bool islast);
void Write_Discover(u8 val, bool islast);


bool Init()
{
    SRAM = NULL;
    Discover_Buffer = NULL;
    return true;
}

void DeInit()
{
    if (SRAM) delete[] SRAM;
    if (Discover_Buffer) delete[] Discover_Buffer;
}

void Reset()
{
    if (SRAM) delete[] SRAM;
    if (Discover_Buffer) delete[] Discover_Buffer;
    SRAM = NULL;
    Discover_Buffer = NULL;
}

void DoSavestate(Savestate* file)
{
    file->Section("NDCS");

    // CHECKME/TODO/whatever
    // should the autodetect status shit go in the savestate???
    // I don't think so.
    // worst case is that the savestate was taken before autodetect took place completely
    // and that causes it to yield a different result, fucking things up
    // but that is unlikely
    // and we should just use a goddamn database anyway, this is a trainwreck

    // we reload the SRAM contents, tho.
    // it should be the same file (as it should be the same ROM, duh)
    // but the contents may change
    // TODO maybe: possibility to save to a separate file when using savestates????

    // also the SRAM size shouldn't change. unless something something autodetect something but fuck that code.

    //if (!file->Saving && SRAMLength)
    //    delete[] SRAM;

    u32 oldlen = SRAMLength;

    file->Var32(&SRAMLength);
    if (SRAMLength != oldlen)
    {
        printf("savestate: VERY BAD!!!! SRAM LENGTH DIFFERENT. %d -> %d\n", oldlen, SRAMLength);
        printf("oh well. loading it anyway. adsfgdsf\n");

        if (oldlen) delete[] SRAM;
        if (SRAMLength) SRAM = new u8[SRAMLength];
    }
    if (SRAMLength)
    {
        //if (!file->Saving)
        //    SRAM = new u8[SRAMLength];

        file->VarArray(SRAM, SRAMLength);
    }

    // SPI status shito

    file->Var32(&Hold);
    file->Var8(&CurCmd);
    file->Var32(&DataPos);
    file->Var8(&Data);

    file->Var8(&StatusReg);
    file->Var32(&Addr);
}

void LoadSave(const char* path)
{
    if (SRAM) delete[] SRAM;
    if (Discover_Buffer) delete[] Discover_Buffer;

    Discover_Buffer = NULL;
    Discover_LastPC = 0;
    Discover_AddrLength = 0x7FFFFFFF;
    Discover_LikelySize = 0;

    strncpy(SRAMPath, path, 1023);
    SRAMPath[1023] = '\0';

    FILE* f = melon_fopen(path, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        SRAMLength = (u32)ftell(f);
        SRAM = new u8[SRAMLength];

        fseek(f, 0, SEEK_SET);
        fread(SRAM, SRAMLength, 1, f);

        fclose(f);

        switch (SRAMLength)
        {
        case 512: WriteFunc = Write_EEPROMTiny; break;
        case 8192:
        case 65536: WriteFunc = Write_EEPROM; break;
        case 256*1024:
        case 512*1024:
        case 1024*1024:
        case 8192*1024: WriteFunc = Write_Flash; break;
        default:
            printf("!! BAD SAVE LENGTH %d\n", SRAMLength);
            WriteFunc = Write_Null;
            break;
        }
    }
    else
    {
        SRAMLength = 0;
        WriteFunc = Write_Discover;
        Discover_MemoryType = 2;
        Discover_Likeliness = 0;

        Discover_DataPos = 0;
        Discover_Buffer = new u8[256*1024];
        memset(Discover_Buffer, 0, 256*1024);
    }

    Hold = 0;
    CurCmd = 0;
    Data = 0;
    StatusReg = 0x00;
}

void RelocateSave(const char* path, bool write)
{
    if (!write)
    {
        LoadSave(path); // lazy
        return;
    }

    strncpy(SRAMPath, path, 1023);
    SRAMPath[1023] = '\0';

    FILE* f = melon_fopen(path, "wb");
    if (!f)
    {
        printf("NDSCart_SRAM::RelocateSave: failed to create new file. fuck\n");
        return;
    }

    fwrite(SRAM, SRAMLength, 1, f);
    fclose(f);
}

u8 Read()
{
    return Data;
}

void SetMemoryType()
{
    SRAMLength = 0;

    if (Discover_LikelySize)
    {
        if (Discover_LikelySize >= 0x40000)
        {
            Discover_MemoryType = 4; // FLASH
            SRAMLength = Discover_LikelySize;
        }
        else if (Discover_LikelySize == 0x10000 || Discover_MemoryType == 3)
        {
            if (Discover_MemoryType > 3)
                printf("incoherent detection result: type=%d size=%X\n", Discover_MemoryType, Discover_LikelySize);

            Discover_MemoryType = 3;
        }
        else if (Discover_LikelySize == 0x2000)
        {
            if (Discover_MemoryType > 3)
                printf("incoherent detection result: type=%d size=%X\n", Discover_MemoryType, Discover_LikelySize);

            Discover_MemoryType = 2;
        }
        else if (Discover_LikelySize == 0x200 || Discover_MemoryType == 1)
        {
            if (Discover_MemoryType > 1)
                printf("incoherent detection result: type=%d size=%X\n", Discover_MemoryType, Discover_LikelySize);

            Discover_MemoryType = 1;
        }
        else
        {
            printf("bad save size %X. assuming EEPROM 64K\n", Discover_LikelySize);
            Discover_MemoryType = 2;
        }
    }

    switch (Discover_MemoryType)
    {
    case 1:
        printf("Save memory type: EEPROM 4k\n");
        WriteFunc = Write_EEPROMTiny;
        SRAMLength = 512;
        break;

    case 2:
        printf("Save memory type: EEPROM 64k\n");
        WriteFunc = Write_EEPROM;
        SRAMLength = 8192;
        break;

    case 3:
        printf("Save memory type: EEPROM 512k\n");
        WriteFunc = Write_EEPROM;
        SRAMLength = 65536;
        break;

    case 4:
        if (!SRAMLength) SRAMLength = 256*1024;
        printf("Save memory type: Flash %dk\n", SRAMLength/1024);
        WriteFunc = Write_Flash;
        break;

    case 5:
        printf("Save memory type: ...something else\n");
        WriteFunc = Write_Null;
        SRAMLength = 0;
        break;
    }

    if (!SRAMLength)
        return;

    SRAM = new u8[SRAMLength];

    // replay writes that occured during discovery
    u8 prev_cmd = CurCmd;
    u32 pos = 0;
    while (pos < 256*1024)
    {
        u32 len = *(u32*)&Discover_Buffer[pos];
        pos += 4;
        if (len == 0) break;

        CurCmd = Discover_Buffer[pos++];
        DataPos = 0;
        Addr = 0;
        Data = 0;
        for (u32 i = 1; i < len; i++)
        {
            WriteFunc(Discover_Buffer[pos++], (i==(len-1)));
            DataPos++;
        }
    }

    CurCmd = prev_cmd;

    delete[] Discover_Buffer;
    Discover_Buffer = NULL;
}

void Write_Discover(u8 val, bool islast)
{
    // attempt at autodetecting the type of save memory.
    // we basically hope the game will be nice and clear whole pages of memory.

    if (CurCmd == 0x03 || CurCmd == 0x0B)
    {
        if (Discover_Likeliness) // writes have occured before this read
        {
            // apply. and pray.
            SetMemoryType();

            DataPos = 0;
            Addr = 0;
            Data = 0;
            return WriteFunc(val, islast);
        }
        else
        {
            if (DataPos == 0)
            {
                Discover_LastPC = NDS::GetPC((NDS::ExMemCnt[0] >> 11) & 0x1);
                Discover_Addr = val;
            }
            else
            {
                bool addrlenchange = false;

                Discover_Addr <<= 8;
                Discover_Addr |= val;

                u32 pc = NDS::GetPC((NDS::ExMemCnt[0] >> 11) & 0x1);
                if ((pc != Discover_LastPC) || islast)
                {
                    if (DataPos < Discover_AddrLength)
                    {
                        Discover_AddrLength = DataPos;
                        addrlenchange = true;
                    }
                }

                if (DataPos == Discover_AddrLength)
                {
                    Discover_Addr >>= 8;

                    // determine the address for this read
                    // the idea is to see how far the game reads
                    // but we need margins for games that have antipiracy
                    // as those will generally read just past the limit
                    // and expect it to wrap to zero

                    u32 likelysize = 0;

                    if (DataPos == 3) // FLASH
                    {
                        if (Discover_Addr >= 0x101000)
                            likelysize = 0x800000; // 8M
                        else if (Discover_Addr >= 0x81000)
                            likelysize = 0x100000; // 1M
                        else if (Discover_Addr >= 0x41000)
                            likelysize = 0x80000; // 512K
                        else
                            likelysize = 0x40000; // 256K
                    }
                    else if (DataPos == 2) // EEPROM
                    {
                        if (Discover_Addr >= 0x3000)
                            likelysize = 0x10000; // 64K
                        else
                            likelysize = 0x2000; // 8K
                    }
                    else if (DataPos == 1) // tiny EEPROM
                    {
                        likelysize = 0x200; // always 4K
                    }

                    if ((likelysize > Discover_LikelySize) || addrlenchange)
                        Discover_LikelySize = likelysize;
                }
            }

            Data = 0;
            return;
        }
    }

    if (CurCmd == 0x02 || CurCmd == 0x0A)
    {
        if (DataPos == 0)
            Discover_Buffer[Discover_DataPos + 4] = CurCmd;

        Discover_Buffer[Discover_DataPos + 5 + DataPos] = val;

        if (islast)
        {
            u32 len = DataPos+1;

            *(u32*)&Discover_Buffer[Discover_DataPos] = len+1;
            Discover_DataPos += 5+len;

            if (Discover_Likeliness <= len)
            {
                Discover_Likeliness = len;

                if (len > 3+256) // bigger Flash, FRAM, whatever
                {
                    Discover_MemoryType = 5;
                }
                else if ((len > 2+128) || (len > 1+16 && CurCmd == 0xA)) // Flash
                {
                    Discover_MemoryType = 4;
                }
                else if (len > 2+32) // EEPROM 512k
                {
                    Discover_MemoryType = 3;
                }
                else if (len > 1+16 || (len != 1+16 && CurCmd != 0x0A)) // EEPROM 64k
                {
                    Discover_MemoryType = 2;
                }
                else // EEPROM 4k
                {
                    Discover_MemoryType = 1;
                }
            }

            printf("discover: type=%d likeliness=%d\n", Discover_MemoryType, Discover_Likeliness);
        }
    }
}

void Write_Null(u8 val, bool islast) {}

void Write_EEPROMTiny(u8 val, bool islast)
{
    switch (CurCmd)
    {
    case 0x02:
    case 0x0A:
        if (DataPos < 1)
        {
            Addr = val;
            Data = 0;
        }
        else
        {
            SRAM[(Addr + ((CurCmd==0x0A)?0x100:0)) & 0x1FF] = val;
            Addr++;
        }
        break;

    case 0x03:
    case 0x0B:
        if (DataPos < 1)
        {
            Addr = val;
            Data = 0;
        }
        else
        {
            Data = SRAM[(Addr + ((CurCmd==0x0B)?0x100:0)) & 0x1FF];
            Addr++;
        }
        break;

    case 0x9F:
        Data = 0xFF;
        break;

    default:
        if (DataPos==0)
            printf("unknown tiny EEPROM save command %02X\n", CurCmd);
        break;
    }
}

void Write_EEPROM(u8 val, bool islast)
{
    switch (CurCmd)
    {
    case 0x02:
        if (DataPos < 2)
        {
            Addr <<= 8;
            Addr |= val;
            Data = 0;
        }
        else
        {
            SRAM[Addr & (SRAMLength-1)] = val;
            Addr++;
        }
        break;

    case 0x03:
        if (DataPos < 2)
        {
            Addr <<= 8;
            Addr |= val;
            Data = 0;
        }
        else
        {
            Data = SRAM[Addr & (SRAMLength-1)];
            Addr++;
        }
        break;

    case 0x9F:
        Data = 0xFF;
        break;

    default:
        if (DataPos==0)
            printf("unknown EEPROM save command %02X\n", CurCmd);
        break;
    }
}

void Write_Flash(u8 val, bool islast)
{
    switch (CurCmd)
    {
    case 0x02:
        if (DataPos < 3)
        {
            Addr <<= 8;
            Addr |= val;
            Data = 0;
        }
        else
        {
            SRAM[Addr & (SRAMLength-1)] = 0;
            Addr++;
        }
        break;

    case 0x03:
        if (DataPos < 3)
        {
            Addr <<= 8;
            Addr |= val;
            Data = 0;
        }
        else
        {
            Data = SRAM[Addr & (SRAMLength-1)];
            Addr++;
        }
        break;

    case 0x0A:
        if (DataPos < 3)
        {
            Addr <<= 8;
            Addr |= val;
            Data = 0;
        }
        else
        {
            SRAM[Addr & (SRAMLength-1)] = val;
            Addr++;
        }
        break;

    case 0x9F:
        Data = 0xFF;
        break;

    case 0xD8:
        if (DataPos < 3)
        {
            Addr <<= 8;
            Addr |= val;
            Data = 0;
        }
        if (DataPos == 2)
        {
            for (u32 i = 0; i < 0x10000; i++)
            {
                SRAM[Addr & (SRAMLength-1)] = 0;
                Addr++;
            }
        }
        break;

    case 0xDB:
        if (DataPos < 3)
        {
            Addr <<= 8;
            Addr |= val;
            Data = 0;
        }
        if (DataPos == 2)
        {
            for (u32 i = 0; i < 0x100; i++)
            {
                SRAM[Addr & (SRAMLength-1)] = 0;
                Addr++;
            }
        }
        break;

    default:
        if (DataPos==0)
            printf("unknown Flash save command %02X\n", CurCmd);
        break;
    }
}

void Write(u8 val, u32 hold)
{
    bool islast = false;

    if (!hold)
    {
        if (Hold) islast = true;
        else CurCmd = val;
        Hold = 0;
    }

    if (hold && (!Hold))
    {
        CurCmd = val;
        Hold = 1;
        Data = 0;
        DataPos = 0;
        Addr = 0;
        //printf("save SPI command %02X\n", CurCmd);
        return;
    }

    switch (CurCmd)
    {
    case 0x00:
        // Pok�mon carts have an IR transceiver thing, and send this
        // to bypass it and access SRAM.
        // TODO: design better
        CurCmd = val;
        break;
    case 0x08:
        // see above
        // TODO: work out how the IR thing works. emulate it.
        Data = 0xAA;
        break;

    case 0x02:
    case 0x03:
    case 0x0A:
    case 0x0B:
    case 0x9F:
    case 0xD8:
    case 0xDB:
        WriteFunc(val, islast);
        DataPos++;
        break;

    case 0x04: // write disable
        StatusReg &= ~(1<<1);
        Data = 0;
        break;

    case 0x05: // read status reg
        Data = StatusReg;
        break;

    case 0x06: // write enable
        StatusReg |= (1<<1);
        Data = 0;
        break;

    default:
        if (DataPos==0)
            printf("unknown save SPI command %02X %02X %d\n", CurCmd, val, islast);
        break;
    }

    if (islast && (CurCmd == 0x02 || CurCmd == 0x0A) && (SRAMLength > 0))
    {
        FILE* f = melon_fopen(SRAMPath, "wb");
        if (f)
        {
            fwrite(SRAM, SRAMLength, 1, f);
            fclose(f);
        }
    }
}

}


namespace NDSCart
{

u16 SPICnt;
u32 ROMCnt;

u8 ROMCommand[8];
u32 ROMDataOut;

u8 DataOut[0x4000];
u32 DataOutPos;
u32 DataOutLen;

bool CartInserted;
u8* CartROM;
u32 CartROMSize;
u32 CartID;
bool CartIsHomebrew;

u32 CmdEncMode;
u32 DataEncMode;

u32 Key1_KeyBuf[0x412];

u64 Key2_X;
u64 Key2_Y;


u32 ByteSwap(u32 val)
{
    return (val >> 24) | ((val >> 8) & 0xFF00) | ((val << 8) & 0xFF0000) | (val << 24);
}

void Key1_Encrypt(u32* data)
{
    u32 y = data[0];
    u32 x = data[1];
    u32 z;

    for (u32 i = 0x0; i <= 0xF; i++)
    {
        z = Key1_KeyBuf[i] ^ x;
        x =  Key1_KeyBuf[0x012 +  (z >> 24)        ];
        x += Key1_KeyBuf[0x112 + ((z >> 16) & 0xFF)];
        x ^= Key1_KeyBuf[0x212 + ((z >>  8) & 0xFF)];
        x += Key1_KeyBuf[0x312 +  (z        & 0xFF)];
        x ^= y;
        y = z;
    }

    data[0] = x ^ Key1_KeyBuf[0x10];
    data[1] = y ^ Key1_KeyBuf[0x11];
}

void Key1_Decrypt(u32* data)
{
    u32 y = data[0];
    u32 x = data[1];
    u32 z;

    for (u32 i = 0x11; i >= 0x2; i--)
    {
        z = Key1_KeyBuf[i] ^ x;
        x =  Key1_KeyBuf[0x012 +  (z >> 24)        ];
        x += Key1_KeyBuf[0x112 + ((z >> 16) & 0xFF)];
        x ^= Key1_KeyBuf[0x212 + ((z >>  8) & 0xFF)];
        x += Key1_KeyBuf[0x312 +  (z        & 0xFF)];
        x ^= y;
        y = z;
    }

    data[0] = x ^ Key1_KeyBuf[0x1];
    data[1] = y ^ Key1_KeyBuf[0x0];
}

void Key1_ApplyKeycode(u32* keycode, u32 mod)
{
    Key1_Encrypt(&keycode[1]);
    Key1_Encrypt(&keycode[0]);

    u32 temp[2] = {0,0};

    for (u32 i = 0; i <= 0x11; i++)
    {
        Key1_KeyBuf[i] ^= ByteSwap(keycode[i % mod]);
    }
    for (u32 i = 0; i <= 0x410; i+=2)
    {
        Key1_Encrypt(temp);
        Key1_KeyBuf[i  ] = temp[1];
        Key1_KeyBuf[i+1] = temp[0];
    }
}

void Key1_InitKeycode(u32 idcode, u32 level, u32 mod)
{
    memcpy(Key1_KeyBuf, &NDS::ARM7BIOS[0x30], 0x1048); // hax

    u32 keycode[3] = {idcode, idcode>>1, idcode<<1};
    if (level >= 1) Key1_ApplyKeycode(keycode, mod);
    if (level >= 2) Key1_ApplyKeycode(keycode, mod);
    if (level >= 3)
    {
        keycode[1] <<= 1;
        keycode[2] >>= 1;
        Key1_ApplyKeycode(keycode, mod);
    }
}


void Key2_Encrypt(u8* data, u32 len)
{
    for (u32 i = 0; i < len; i++)
    {
        Key2_X = (((Key2_X >> 5) ^
                   (Key2_X >> 17) ^
                   (Key2_X >> 18) ^
                   (Key2_X >> 31)) & 0xFF)
                 + (Key2_X << 8);
        Key2_Y = (((Key2_Y >> 5) ^
                   (Key2_Y >> 23) ^
                   (Key2_Y >> 18) ^
                   (Key2_Y >> 31)) & 0xFF)
                 + (Key2_Y << 8);

        Key2_X &= 0x0000007FFFFFFFFFULL;
        Key2_Y &= 0x0000007FFFFFFFFFULL;
    }
}


bool Init()
{
    if (!NDSCart_SRAM::Init()) return false;

    CartROM = NULL;

    return true;
}

void DeInit()
{
    if (CartROM) delete[] CartROM;

    NDSCart_SRAM::DeInit();
}

void Reset()
{
    SPICnt = 0;
    ROMCnt = 0;

    memset(ROMCommand, 0, 8);
    ROMDataOut = 0;

    Key2_X = 0;
    Key2_Y = 0;

    memset(DataOut, 0, 0x4000);
    DataOutPos = 0;
    DataOutLen = 0;

    CartInserted = false;
    if (CartROM) delete[] CartROM;
    CartROM = NULL;
    CartROMSize = 0;
    CartID = 0;
    CartIsHomebrew = false;

    CmdEncMode = 0;
    DataEncMode = 0;

    NDSCart_SRAM::Reset();
}

void DoSavestate(Savestate* file)
{
    file->Section("NDSC");

    file->Var16(&SPICnt);
    file->Var32(&ROMCnt);

    file->VarArray(ROMCommand, 8);
    file->Var32(&ROMDataOut);

    file->VarArray(DataOut, 0x4000);
    file->Var32(&DataOutPos);
    file->Var32(&DataOutLen);

    // cart inserted/len/ROM/etc should be already populated
    // savestate should be loaded after the right game is loaded
    // (TODO: system to verify that indeed the right ROM is loaded)
    // (what to CRC? whole ROM? code binaries? latter would be more convenient for ie. romhaxing)

    file->Var32(&CmdEncMode);
    file->Var32(&DataEncMode);

    // TODO: check KEY1 shit??

    NDSCart_SRAM::DoSavestate(file);
}


void ApplyDLDIPatch()
{
    // TODO: embed patches? let the user choose? default to some builtin driver?

    u32 offset = *(u32*)&CartROM[0x20];
    u32 size = *(u32*)&CartROM[0x2C];

    u8* binary = &CartROM[offset];
    u32 dldioffset = 0;

    for (u32 i = 0; i < size; i++)
    {
        if (*(u32*)&binary[i  ] == 0xBF8DA5ED &&
            *(u32*)&binary[i+4] == 0x69684320 &&
            *(u32*)&binary[i+8] == 0x006D6873)
        {
            dldioffset = i;
            break;
        }
    }

    if (!dldioffset)
    {
        return;
    }

    printf("DLDI shit found at %08X (%08X)\n", dldioffset, offset+dldioffset);

    FILE* f = fopen("dldi.bin", "rb");
    if (!f)
    {
        printf("no DLDI patch available. oh well\n");
        return;
    }

    u32 dldisize;
    fseek(f, 0, SEEK_END);
    dldisize = ftell(f);
    fseek(f, 0, SEEK_SET);

    u8* patch = new u8[dldisize];
    fread(patch, dldisize, 1, f);
    fclose(f);

    if (*(u32*)&patch[0] != 0xBF8DA5ED ||
        *(u32*)&patch[4] != 0x69684320 ||
        *(u32*)&patch[8] != 0x006D6873)
    {
        printf("bad DLDI patch\n");
        delete[] patch;
        return;
    }

    if (patch[0x0D] > binary[dldioffset+0x0F])
    {
        printf("DLDI driver ain't gonna fit, sorry\n");
        delete[] patch;
        return;
    }

    printf("existing driver is: %s\n", &binary[dldioffset+0x10]);
    printf("new driver is: %s\n", &patch[0x10]);

    u32 memaddr = *(u32*)&binary[dldioffset+0x40];
    if (memaddr == 0)
        memaddr = *(u32*)&binary[dldioffset+0x68] - 0x80;

    u32 patchbase = *(u32*)&patch[0x40];
    u32 delta = memaddr - patchbase;

    u32 patchsize = 1 << patch[0x0D];
    u32 patchend = patchbase + patchsize;

    memcpy(&binary[dldioffset], patch, dldisize);

    *(u32*)&binary[dldioffset+0x40] += delta;
    *(u32*)&binary[dldioffset+0x44] += delta;
    *(u32*)&binary[dldioffset+0x48] += delta;
    *(u32*)&binary[dldioffset+0x4C] += delta;
    *(u32*)&binary[dldioffset+0x50] += delta;
    *(u32*)&binary[dldioffset+0x54] += delta;
    *(u32*)&binary[dldioffset+0x58] += delta;
    *(u32*)&binary[dldioffset+0x5C] += delta;

    *(u32*)&binary[dldioffset+0x68] += delta;
    *(u32*)&binary[dldioffset+0x6C] += delta;
    *(u32*)&binary[dldioffset+0x70] += delta;
    *(u32*)&binary[dldioffset+0x74] += delta;
    *(u32*)&binary[dldioffset+0x78] += delta;
    *(u32*)&binary[dldioffset+0x7C] += delta;

    u8 fixmask = patch[0x0E];

    if (fixmask & 0x01)
    {
        u32 fixstart = *(u32*)&patch[0x40] - patchbase;
        u32 fixend = *(u32*)&patch[0x44] - patchbase;

        for (u32 addr = fixstart; addr < fixend; addr+=4)
        {
            u32 val = *(u32*)&binary[dldioffset+addr];
            if (val >= patchbase && val < patchend)
                *(u32*)&binary[dldioffset+addr] += delta;
        }
    }
    if (fixmask & 0x02)
    {
        u32 fixstart = *(u32*)&patch[0x48] - patchbase;
        u32 fixend = *(u32*)&patch[0x4C] - patchbase;

        for (u32 addr = fixstart; addr < fixend; addr+=4)
        {
            u32 val = *(u32*)&binary[dldioffset+addr];
            if (val >= patchbase && val < patchend)
                *(u32*)&binary[dldioffset+addr] += delta;
        }
    }
    if (fixmask & 0x04)
    {
        u32 fixstart = *(u32*)&patch[0x50] - patchbase;
        u32 fixend = *(u32*)&patch[0x54] - patchbase;

        for (u32 addr = fixstart; addr < fixend; addr+=4)
        {
            u32 val = *(u32*)&binary[dldioffset+addr];
            if (val >= patchbase && val < patchend)
                *(u32*)&binary[dldioffset+addr] += delta;
        }
    }
    if (fixmask & 0x08)
    {
        u32 fixstart = *(u32*)&patch[0x58] - patchbase;
        u32 fixend = *(u32*)&patch[0x5C] - patchbase;

        memset(&binary[dldioffset+fixstart], 0, fixend-fixstart);
    }

    delete[] patch;
    printf("applied DLDI patch\n");
}


bool LoadROM(const char* path, const char* sram, bool direct)
{
    // TODO: streaming mode? for really big ROMs or systems with limited RAM
    // for now we're lazy

    FILE* f = melon_fopen(path, "rb");
    if (!f)
    {
        return false;
    }

    NDS::Reset();

    fseek(f, 0, SEEK_END);
    u32 len = (u32)ftell(f);

    CartROMSize = 0x200;
    while (CartROMSize < len)
        CartROMSize <<= 1;

    u32 gamecode;
    fseek(f, 0x0C, SEEK_SET);
    fread(&gamecode, 4, 1, f);

    CartROM = new u8[CartROMSize];
    memset(CartROM, 0, CartROMSize);
    fseek(f, 0, SEEK_SET);
    fread(CartROM, 1, len, f);

    fclose(f);
    //CartROM = f;

    // generate a ROM ID
    // note: most games don't check the actual value
    // it just has to stay the same throughout gameplay
    CartID = 0x00001FC2;

    if (*(u32*)&CartROM[0x20] < 0x4000)
    {
        //ApplyDLDIPatch();
    }

    if (direct)
    {
        // TODO: in the case of an already-encrypted secure area, direct boot
        // needs it decrypted
        NDS::SetupDirectBoot();
        CmdEncMode = 2;
    }

    CartInserted = true;

    u32 arm9base = *(u32*)&CartROM[0x20];
    if (arm9base < 0x8000)
    {
        if (arm9base >= 0x4000)
        {
            // reencrypt secure area if needed
            if (*(u32*)&CartROM[arm9base] == 0xE7FFDEFF && *(u32*)&CartROM[arm9base+0x10] != 0xE7FFDEFF)
            {
                printf("Re-encrypting cart secure area\n");

                strncpy((char*)&CartROM[arm9base], "encryObj", 8);

                Key1_InitKeycode(gamecode, 3, 2);
                for (u32 i = 0; i < 0x800; i += 8)
                    Key1_Encrypt((u32*)&CartROM[arm9base + i]);

                Key1_InitKeycode(gamecode, 2, 2);
                Key1_Encrypt((u32*)&CartROM[arm9base]);
            }
        }
        else
            CartIsHomebrew = true;
    }

    // encryption
    Key1_InitKeycode(gamecode, 2, 2);


    // save
    printf("Save file: %s\n", sram);
    NDSCart_SRAM::LoadSave(sram);

    return true;
}

void RelocateSave(const char* path, bool write)
{
    // herp derp
    NDSCart_SRAM::RelocateSave(path, write);
}

void ReadROM(u32 addr, u32 len, u32 offset)
{
    if (!CartInserted) return;

    if (addr >= CartROMSize) return;
    if ((addr+len) > CartROMSize)
        len = CartROMSize - addr;

    memcpy(DataOut+offset, CartROM+addr, len);
}

void ReadROM_B7(u32 addr, u32 len, u32 offset)
{
    if (!CartInserted) return;

    addr &= (CartROMSize-1);
    if (!CartIsHomebrew)
    {
        if (addr < 0x8000)
            addr = 0x8000 + (addr & 0x1FF);
    }

    memcpy(DataOut+offset, CartROM+addr, len);
}


void ROMEndTransfer(u32 param)
{
    ROMCnt &= ~(1<<31);

    if (SPICnt & (1<<14))
        NDS::SetIRQ((NDS::ExMemCnt[0]>>11)&0x1, NDS::IRQ_CartSendDone);
}

void ROMPrepareData(u32 param)
{
    if (DataOutPos >= DataOutLen)
        ROMDataOut = 0;
    else
        ROMDataOut = *(u32*)&DataOut[DataOutPos];

    DataOutPos += 4;

    ROMCnt |= (1<<23);

    if (NDS::ExMemCnt[0] & (1<<11))
        NDS::CheckDMAs(1, 0x12);
    else
        NDS::CheckDMAs(0, 0x05);
}

u32 sc_addr = 0;

void WriteROMCnt(u32 val)
{
    ROMCnt = (val & 0xFF7F7FFF) | (ROMCnt & 0x00800000);

    if (!(SPICnt & (1<<15))) return;

    if (val & (1<<15))
    {
        u32 snum = (NDS::ExMemCnt[0]>>8)&0x8;
        u64 seed0 = *(u32*)&NDS::ROMSeed0[snum] | ((u64)NDS::ROMSeed0[snum+4] << 32);
        u64 seed1 = *(u32*)&NDS::ROMSeed1[snum] | ((u64)NDS::ROMSeed1[snum+4] << 32);

        Key2_X = 0;
        Key2_Y = 0;
        for (u32 i = 0; i < 39; i++)
        {
            if (seed0 & (1ULL << i)) Key2_X |= (1ULL << (38-i));
            if (seed1 & (1ULL << i)) Key2_Y |= (1ULL << (38-i));
        }

        printf("seed0: %02X%08X\n", (u32)(seed0>>32), (u32)seed0);
        printf("seed1: %02X%08X\n", (u32)(seed1>>32), (u32)seed1);
        printf("key2 X: %02X%08X\n", (u32)(Key2_X>>32), (u32)Key2_X);
        printf("key2 Y: %02X%08X\n", (u32)(Key2_Y>>32), (u32)Key2_Y);
    }

    if (!(ROMCnt & (1<<31))) return;

    u32 datasize = (ROMCnt >> 24) & 0x7;
    if (datasize == 7)
        datasize = 4;
    else if (datasize > 0)
        datasize = 0x100 << datasize;

    DataOutPos = 0;
    DataOutLen = datasize;

    // handle KEY1 encryption as needed.
    // KEY2 encryption is implemented in hardware and doesn't need to be handled.
    u8 cmd[8];
    if (CmdEncMode == 1)
    {
        *(u32*)&cmd[0] = ByteSwap(*(u32*)&ROMCommand[4]);
        *(u32*)&cmd[4] = ByteSwap(*(u32*)&ROMCommand[0]);
        Key1_Decrypt((u32*)cmd);
        u32 tmp = ByteSwap(*(u32*)&cmd[4]);
        *(u32*)&cmd[4] = ByteSwap(*(u32*)&cmd[0]);
        *(u32*)&cmd[0] = tmp;
    }
    else
    {
        *(u32*)&cmd[0] = *(u32*)&ROMCommand[0];
        *(u32*)&cmd[4] = *(u32*)&ROMCommand[4];
    }

    /*printf("ROM COMMAND %04X %08X %02X%02X%02X%02X%02X%02X%02X%02X SIZE %04X\n",
           SPICnt, ROMCnt,
           cmd[0], cmd[1], cmd[2], cmd[3],
           cmd[4], cmd[5], cmd[6], cmd[7],
           datasize);*/

    switch (cmd[0])
    {
    case 0x9F:
        memset(DataOut, 0xFF, DataOutLen);
        break;

    case 0x00:
        memset(DataOut, 0, DataOutLen);
        if (DataOutLen > 0x1000)
        {
            ReadROM(0, 0x1000, 0);
            for (u32 pos = 0x1000; pos < DataOutLen; pos += 0x1000)
                memcpy(DataOut+pos, DataOut, 0x1000);
        }
        else
            ReadROM(0, DataOutLen, 0);
        break;

    case 0x90:
    case 0xB8:
        for (u32 pos = 0; pos < DataOutLen; pos += 4)
            *(u32*)&DataOut[pos] = CartID;
        break;

    case 0x3C:
        if (CartInserted) CmdEncMode = 1;
        break;

    case 0xB7:
        {
            u32 addr = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            memset(DataOut, 0, DataOutLen);

            if (((addr + DataOutLen - 1) >> 12) != (addr >> 12))
            {
                u32 len1 = 0x1000 - (addr & 0xFFF);
                ReadROM_B7(addr, len1, 0);
                ReadROM_B7(addr+len1, DataOutLen-len1, len1);
            }
            else
                ReadROM_B7(addr, DataOutLen, 0);
        }
        break;


    // SUPERCARD EMULATION TEST
    // TODO: INTEGRATE BETTER!!!!

    case 0x70: // init??? returns whether SDHC addressing should be used
        for (u32 pos = 0; pos < DataOutLen; pos += 4)
            *(u32*)&DataOut[pos] = 0;
        break;

    case 0x53: // set address for read
        sc_addr = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
        printf("SUPERCARD: read %08X\n", sc_addr);
        break;

    case 0x80: // read operation busy, I guess
        // TODO: make it take some time
        for (u32 pos = 0; pos < DataOutLen; pos += 4)
            *(u32*)&DataOut[pos] = 0;
        break;

    case 0x81: // read data
        {
            if (DataOutLen != 0x200)
                printf("SUPERCARD: BOGUS READ %d\n", DataOutLen);

            // TODO: this is really inefficient. just testing
            FILE* f = fopen("scsd.bin", "rb");
            fseek(f, sc_addr, SEEK_SET);
            fread(DataOut, 1, 0x200, f);
            fclose(f);
        }
        break;

    // SUPERCARD EMULATION TEST END


    default:
        switch (cmd[0] & 0xF0)
        {
        case 0x40:
            DataEncMode = 2;
            break;

        case 0x10:
            for (u32 pos = 0; pos < DataOutLen; pos += 4)
                *(u32*)&DataOut[pos] = CartID;
            break;

        case 0x20:
            {
                u32 addr = (cmd[2] & 0xF0) << 8;
                ReadROM(addr, 0x1000, 0);
            }
            break;

        case 0xA0:
            CmdEncMode = 2;
            break;
        }
        break;
    }

    ROMCnt &= ~(1<<23);

    // ROM transfer timings
    // the bus is parallel with 8 bits
    // thus a command would take 8 cycles to be transferred
    // and it would take 4 cycles to receive a word of data
    // TODO: advance read position if bit28 is set

    u32 xfercycle = (ROMCnt & (1<<27)) ? 8 : 5;
    u32 cmddelay = 8 + (ROMCnt & 0x1FFF);
    if (datasize) cmddelay += ((ROMCnt >> 16) & 0x3F);

    if (datasize == 0)
        NDS::ScheduleEvent(NDS::Event_ROMTransfer, false, xfercycle*cmddelay, ROMEndTransfer, 0);
    else
        NDS::ScheduleEvent(NDS::Event_ROMTransfer, true, xfercycle*(cmddelay+4), ROMPrepareData, 0);
}

u32 ReadROMData()
{
    if (ROMCnt & (1<<23))
    {
        ROMCnt &= ~(1<<23);

        if (DataOutPos < DataOutLen)
        {
            u32 xfercycle = (ROMCnt & (1<<27)) ? 8 : 5;
            u32 delay = 4;
            if (!(DataOutPos & 0x1FF)) delay += ((ROMCnt >> 16) & 0x3F);

            NDS::ScheduleEvent(NDS::Event_ROMTransfer, true, xfercycle*delay, ROMPrepareData, 0);
        }
        else
            ROMEndTransfer(0);
    }

    return ROMDataOut;
}


void WriteSPICnt(u16 val)
{
    SPICnt = (SPICnt & 0x0080) | (val & 0xE043);
    if (SPICnt & (1<<7))
        printf("!! CHANGING AUXSPICNT DURING TRANSFER: %04X\n", val);
}

void SPITransferDone(u32 param)
{
    SPICnt &= ~(1<<7);
}

u8 ReadSPIData()
{
    if (!(SPICnt & (1<<15))) return 0;
    if (!(SPICnt & (1<<13))) return 0;
    if (SPICnt & (1<<7)) return 0; // checkme

    return NDSCart_SRAM::Read();
}

void WriteSPIData(u8 val)
{
    if (!(SPICnt & (1<<15))) return;
    if (!(SPICnt & (1<<13))) return;

    if (SPICnt & (1<<7)) printf("!! WRITING AUXSPIDATA DURING PENDING TRANSFER\n");

    SPICnt |= (1<<7);
    NDSCart_SRAM::Write(val, SPICnt&(1<<6));

    // SPI transfers one bit per cycle -> 8 cycles per byte
    u32 delay = 8 * (8 << (SPICnt & 0x3));
    NDS::ScheduleEvent(NDS::Event_ROMSPITransfer, false, delay, SPITransferDone, 0);
}

}
