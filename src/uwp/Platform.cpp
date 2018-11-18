/*
    Copyright 2018-2019 pent0

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

#include <ppltasks.h>
#include <Platform.h>

using namespace Windows::Devices::Enumeration;

struct ThreadData
{
    concurrency::task<void> emu_task;
};

namespace Platform 
{

void *Thread_Create(void(*func)())
{
    ThreadData *data = new ThreadData;
    data->emu_task = concurrency::create_task(concurrency::create_async(func));

    return data;
}

void Thread_Wait(void *thread)
{
    ThreadData *data = reinterpret_cast<ThreadData*>(thread);
    data->emu_task.wait();
}

void Thread_Free(void *thread)
{
    delete (ThreadData*)(thread);
}

void* Semaphore_Create()
{
    return CreateSemaphoreA(NULL, 0, 100, NULL);
}

void Semaphore_Free(void* sema)
{
    CloseHandle(sema);
}

void Semaphore_Reset(void* sema)
{
    // while (WaitForSingleObject(sema, INFINITE));
}
void Semaphore_Wait(void* sema)
{
    WaitForSingleObject(sema, INFINITE);
}

void Semaphore_Post(void* sema)
{
    LONG prevCount = 0;
    ReleaseSemaphore(sema, 1, &prevCount);
}

bool MP_Init()
{
    return true;
}

void MP_DeInit()
{

}

int MP_SendPacket(u8* data, int len)
{
    return 0;
}

int MP_RecvPacket(u8* data, bool block)
{
    return 0;
}

// LAN comm interface
// packet type: Ethernet (802.3)
bool LAN_Init()
{
    return true;
}

void LAN_DeInit()
{

}

int LAN_SendPacket(u8* data, int len)
{
    return 0;
}

int LAN_RecvPacket(u8* data)
{
    return 0;
}

void StopEmu()
{

}

}