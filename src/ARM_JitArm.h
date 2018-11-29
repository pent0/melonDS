#ifndef ARM_JITARM_H
#define ARM_JITARM_H

#include "types.h"
#include "NDS.h"
#include "CP15.h"
#include "ARMBase.h"

#include <Arme/Arme.h>
#include <memory>

class ARM_JITARM: public ARM_Base
{
public:
    explicit ARM_JITARM(u32 num);

    void Halt(u32 halt) override;
    void DoSavestate(Savestate* file) override;
    void Reset() override;

    s32 Execute() override;

    void JumpTo(u32 addr, bool restorecpsr = false) override;

    NDS::MemRegion &GetCodeRegion()
    {
        return CodeMem;
    }

    u32 GetCPUNum() const
    {
        return Num;
    }

    u32  GetRegister(u16 id) override
    {
        assert((id <= 15) && (id >= 0) && "Register index out of range!");
        return Inst->state.regs[id];
    }

    void SetRegister(u16 id, u32 value) override
    {
        assert((id <= 15) && (id >= 0) && "Register index out of range!");
        Inst->state.regs[id] = value;
    }

    u32  GetCpsr() override
    {
        return Inst->state.cpsr;
    }

    void SetCpsr(u32 value) override
    {
        Inst->state.cpsr = value;
    }

    u32  GetIRQ(u16 id) override
    {
        return Inst->state.irq[id];
    }

    void SetIRQ(u16 id, u32 val) override
    {
        Inst->state.irq[id] = val;
    }

    u32  GetSVC(u16 id) override
    {
        return Inst->state.svc[id];
    }

    void SetSVC(u16 id, u32 val) override
    {
        Inst->state.svc[id] = val;
    }

    s32  &CPUCyclesToRun() override 
    {
        return CyclesToRun;
    }

    s32  &CPUCycles() override
    {
        return Cycles;
    }

    s32 Cycles;
    s32 CyclesToRun;

    u32 ExceptionBase;

    s32 Waitstates[4][16];

private:
    std::shared_ptr<arme::jit>  Inst;
    u32                         Num;

    NDS::MemRegion              CodeMem;
};

#endif