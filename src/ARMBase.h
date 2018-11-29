#pragma once

#include "types.h"

class Savestate;

class ARM_Base
{
protected:
    u32 ClockShift;
    u32 ClockDiffMask;

public:
    virtual void Halt(u32 halt) = 0;
    virtual void Reset() = 0;

    virtual void SetClockShift(u32 shift)
    {
        ClockShift = shift;
        ClockDiffMask = (1 << shift) - 1;
    }

    virtual void DoSavestate(Savestate* file) = 0;
    virtual s32  Execute() = 0;

    virtual void JumpTo(u32 addr, bool restorecpsr = false) = 0;

    virtual u32  GetRegister(u16 id) = 0;
    virtual void SetRegister(u16 id, u32 value) = 0;

    virtual u32  GetCpsr() = 0;
    virtual void SetCpsr(u32 value) = 0;

    virtual u32  GetIRQ(u16 id) = 0;
    virtual void SetIRQ(u16 id, u32 val) = 0;

    virtual u32  GetSVC(u16 id) = 0;
    virtual void SetSVC(u16 id, u32 val) = 0;

    virtual s32  &CPUCyclesToRun() = 0;
    virtual s32  &CPUCycles() = 0;
};