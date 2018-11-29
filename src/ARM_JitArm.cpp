#include "ARM_JitArm.h"
#include <algorithm>

static u8 DataRead8(void *userdata, u32 addr)
{
    ARM_JITARM *jit = reinterpret_cast<ARM_JITARM*>(userdata);

    u8 val;
    if (!jit->GetCPUNum())
    {
        if (!CP15::HandleDataRead8(addr, &val))
            val = NDS::ARM9Read8(addr);
    }
    else
        val = NDS::ARM7Read8(addr);

    jit->Cycles += jit->Waitstates[2][(addr >> 24) & 0xF];
    return val;
}

static u16 DataRead16(void *userdata, u32 addr)
{
    ARM_JITARM *jit = reinterpret_cast<ARM_JITARM*>(userdata);

    u16 val;

    if (!jit->GetCPUNum())
    {
        if (!CP15::HandleDataRead16(addr, &val))
            val = NDS::ARM9Read16(addr);
    }
    else
        val = NDS::ARM7Read16(addr);

    jit->Cycles += jit->Waitstates[2][(addr >> 24) & 0xF];
    return val;
}

static u32 DataRead32(void *userdata, u32 addr)
{
    ARM_JITARM *jit = reinterpret_cast<ARM_JITARM*>(userdata);

    u32 val;

    if (!jit->GetCPUNum())
    {
        if (!CP15::HandleDataRead32(addr, &val))
            val = NDS::ARM9Read32(addr);
    }
    else
        val = NDS::ARM7Read32(addr);

    jit->Cycles += jit->Waitstates[3][(addr >> 24) & 0xF];

    return val;
}

static void DataWrite8(void *userdata, u32 addr, u8 val)
{
    ARM_JITARM *jit = reinterpret_cast<ARM_JITARM*>(userdata);

    if (!jit->GetCPUNum())
    {
        if (!CP15::HandleDataWrite8(addr, val))
            NDS::ARM9Write8(addr, val);
    }
    else
        NDS::ARM7Write8(addr, val);

    jit->Cycles += jit->Waitstates[2][(addr >> 24) & 0xF];
}

static void DataWrite16(void *userdata, u32 addr, u16 val)
{
    addr &= ~1;
    ARM_JITARM *jit = reinterpret_cast<ARM_JITARM*>(userdata);

    if (!jit->GetCPUNum())
    {
        if (!CP15::HandleDataWrite16(addr, val))
            NDS::ARM9Write16(addr, val);
    }
    else
        NDS::ARM7Write16(addr, val);

    jit->Cycles += jit->Waitstates[2][(addr >> 24) & 0xF];
}

static void DataWrite32(void *userdata, u32 addr, u32 val)
{
    ARM_JITARM *jit = reinterpret_cast<ARM_JITARM*>(userdata);

    addr &= ~3;
    if (!jit->GetCPUNum())
    {
        if (!CP15::HandleDataWrite32(addr, val))
            NDS::ARM9Write32(addr, val);
    }
    else
        NDS::ARM7Write32(addr, val);

    jit->Cycles += jit->Waitstates[3][(addr >> 24) & 0xF];
}

static u32 CodeRead32(void *userdata, u32 addr)
{
    ARM_JITARM *jit = reinterpret_cast<ARM_JITARM*>(userdata);
    jit->Cycles += jit->Waitstates[1][(addr >> 24) & 0xF];

    NDS::MemRegion &codeRegion = jit->GetCodeRegion();

    if (codeRegion.Mem) return *(u32*)&codeRegion.Mem[addr & codeRegion.Mask];

    u32 val;

    if (!jit->GetCPUNum())
    {
        if (!CP15::HandleCodeRead32(addr, &val))
            val = NDS::ARM9Read32(addr);
    }
    else
        val = NDS::ARM7Read32(addr);

    return val;
}

static u32 CpRead(void *userdata, u32 id)
{
    return CP15::Read(id);
}

static void CpWrite(void *userdata, u32 id, u32 val)
{
    return CP15::Write(id, val);
}

static u32 GetRemainingTicks(void *userdata)
{
    ARM_JITARM *jit = reinterpret_cast<ARM_JITARM*>(userdata);
    return jit->CyclesToRun - jit->Cycles;
}

static void AddTicks(void *userdata, u32 ticks)
{
    ARM_JITARM *jit = reinterpret_cast<ARM_JITARM*>(userdata);
    jit->Cycles = std::max((s32)(jit->Cycles + ticks), jit->CyclesToRun);
}

void ARM_JITARM::Halt(u32 halt)
{
    Inst->stop();
}

void ARM_JITARM::DoSavestate(Savestate* file)
{
    file->Section((char*)(Num ? "ARM7" : "ARM9"));

    file->Var32((u32*)&Inst->state.cycles_left);
    file->Var32((u32*)&Inst->state.cycles_to_run);
    //file->Var32(&Inst->state.should_stop);

    file->VarArray(Inst->state.regs, 16 * sizeof(u32));
    file->Var32(&Inst->state.cpsr);
    file->VarArray(Inst->state.fiq, 8 * sizeof(u32));
    file->VarArray(Inst->state.svc, 3 * sizeof(u32));
    file->VarArray(Inst->state.abt, 3 * sizeof(u32));
    file->VarArray(Inst->state.irq, 3 * sizeof(u32));
    file->VarArray(Inst->state.und, 3 * sizeof(u32));
}

ARM_JITARM::ARM_JITARM(u32 num)
    : Num(num)
{
    SetClockShift(0);

    arme::jit_callback callback;
    callback.read_mem8 = DataRead8;
    callback.read_mem16 = DataRead16;
    callback.read_mem32 = DataRead32;
    callback.write_mem8 = DataWrite8;
    callback.write_mem16 = DataWrite16;
    callback.write_mem32 = DataWrite32;
    callback.read_code32 = CodeRead32;
    callback.add_cycles = AddTicks;
    callback.get_remaining_cycles = GetRemainingTicks;
    callback.cp_read = CpRead;
    callback.cp_write = CpWrite;

    callback.userdata = this;

    Inst = std::make_shared<arme::jit>(callback);

    for (int i = 0; i < 16; i++)
    {
        Waitstates[0][i] = 1;
        Waitstates[1][i] = 1;
        Waitstates[2][i] = 1;
        Waitstates[3][i] = 1;
    }

    if (!num)
    {
        // ARM9
        Waitstates[0][0x2] = 1; // main RAM timing, assuming cache hit
        Waitstates[0][0x3] = 4;
        Waitstates[0][0x4] = 4;
        Waitstates[0][0x5] = 5;
        Waitstates[0][0x6] = 5;
        Waitstates[0][0x7] = 4;
        Waitstates[0][0x8] = 19;
        Waitstates[0][0x9] = 19;
        Waitstates[0][0xF] = 4;

        Waitstates[1][0x2] = 1;
        Waitstates[1][0x3] = 8;
        Waitstates[1][0x4] = 8;
        Waitstates[1][0x5] = 10;
        Waitstates[1][0x6] = 10;
        Waitstates[1][0x7] = 8;
        Waitstates[1][0x8] = 38;
        Waitstates[1][0x9] = 38;
        Waitstates[1][0xF] = 8;

        Waitstates[2][0x2] = 1;
        Waitstates[2][0x3] = 2;
        Waitstates[2][0x4] = 2;
        Waitstates[2][0x5] = 2;
        Waitstates[2][0x6] = 2;
        Waitstates[2][0x7] = 2;
        Waitstates[2][0x8] = 12;
        Waitstates[2][0x9] = 12;
        Waitstates[2][0xA] = 20;
        Waitstates[2][0xF] = 2;

        Waitstates[3][0x2] = 1;
        Waitstates[3][0x3] = 2;
        Waitstates[3][0x4] = 2;
        Waitstates[3][0x5] = 4;
        Waitstates[3][0x6] = 4;
        Waitstates[3][0x7] = 2;
        Waitstates[3][0x8] = 24;
        Waitstates[3][0x9] = 24;
        Waitstates[3][0xA] = 20;
        Waitstates[3][0xF] = 2;
    }
    else
    {
        // ARM7
        Waitstates[0][0x0] = 1;
        Waitstates[0][0x2] = 1;
        Waitstates[0][0x3] = 1;
        Waitstates[0][0x4] = 1;
        Waitstates[0][0x6] = 1;
        Waitstates[0][0x8] = 6;
        Waitstates[0][0x9] = 6;

        Waitstates[1][0x0] = 1;
        Waitstates[1][0x2] = 2;
        Waitstates[1][0x3] = 1;
        Waitstates[1][0x4] = 1;
        Waitstates[1][0x6] = 2;
        Waitstates[1][0x8] = 12;
        Waitstates[1][0x9] = 12;

        Waitstates[2][0x0] = 1;
        Waitstates[2][0x2] = 1;
        Waitstates[2][0x3] = 1;
        Waitstates[2][0x4] = 1;
        Waitstates[2][0x6] = 1;
        Waitstates[2][0x8] = 6;
        Waitstates[2][0x9] = 6;
        Waitstates[2][0xA] = 10;

        Waitstates[3][0x0] = 1;
        Waitstates[3][0x2] = 2;
        Waitstates[3][0x3] = 1;
        Waitstates[3][0x4] = 1;
        Waitstates[3][0x6] = 2;
        Waitstates[3][0x8] = 12;
        Waitstates[3][0x9] = 12;
        Waitstates[3][0xA] = 10;
    }
}

s32 ARM_JITARM::Execute()
{
    Cycles = 0;
    Inst->execute();

    s32 executed = Inst->state.cycles_to_run - Inst->state.cycles_left;
    NDS::RunTimingCriticalDevices(Num, executed >> ClockShift);

    return executed;
}

void ARM_JITARM::Reset()
{
    Inst->reset();
    ExceptionBase = Num ? 0x00000000 : 0xFFFF0000;
}

void ARM_JITARM::JumpTo(u32 addr, bool restorecpsr)
{
    Inst->recompiler.visitor.set_pc(addr);
    Inst->state.regs[15] = (addr & 1) ? (addr - 1) : addr;

    // Not restore cpsr yet
}