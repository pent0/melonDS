#pragma once

#include <cassert>
#include <cstdint>
#include <unordered_map>

#include "ArmEmitter.h"

#include <capstone/capstone.h>

#define UNREACHABLE(msg) assert(false & msg)

namespace arme
{

using address = std::uint32_t;

typedef std::uint8_t (*read_mem8_func)(void*, std::uint32_t );
typedef std::uint16_t (*read_mem16_func)(void*, std::uint32_t );
typedef std::uint32_t (*read_mem32_func)(void*, std::uint32_t );

typedef void          (*write_mem8_func)(void*, std::uint32_t, std::uint8_t);
typedef void          (*write_mem16_func)(void*, std::uint32_t, std::uint16_t);
typedef void          (*write_mem32_func)(void*, std::uint32_t, std::uint32_t);
typedef void*         (*alloc_mem_code_func)(std::uint32_t);
typedef void          (*disable_mem_code_writing_func)(void*);
typedef void          (*enable_mem_code_writing_func)(void*);
typedef void          (*free_mem_code_func)(std::uint32_t);
typedef void         (*unhandled_instruction_func)(void*);

struct location_descriptor
{
    address         pc;
    std::uint32_t   cpsr;
    std::uint32_t   fpcsr;

    bool is_thumb()
    {
        return cpsr & 0x20;
    }

};

struct jit_callback
{
    void                   *userdata;

    read_mem8_func          read_mem8;
    read_mem16_func         read_mem16;
    read_mem32_func         read_mem32;
    
    write_mem8_func         write_mem8;
    write_mem16_func        write_mem16;
    write_mem32_func        write_mem32;

    disable_mem_code_writing_func   disable_mem_code_writing;
    enable_mem_code_writing_func    enable_mem_code_writing;
    free_mem_code_func      free_mem_code;
};

static ARMReg cs_arm_op_to_reg(cs_arm_op &op)
{
    assert((op.type == ARM_OP_REG) & "The Capstone operand is not an register!");
    return static_cast<ARMReg>((op.reg - ARM_REG_R0) + ARMReg::R0);
}

static Operand2 cs_arm_op_to_operand2(cs_arm_op &op)
{
    switch (op.type)
    {
    case ARM_OP_REG: 
    {
        ARMReg reg = cs_arm_op_to_reg(op);
        ShiftType st = 0;
        
        if (op.shift.type != ARM_SFT_INVALID)
        {
            switch (op.shift.type)
            {
            case ARM_SFT_ASR_REG:
            {
                st = ShiftType::ST_ASR;
                break;
            }

            case ARM_SFT_LSL_REG:
            {
                st = ShiftType::ST_LSL;
                break;
            }

            case ARM_SFT_LSR_REG:
            {
                st = ShiftType::ST_LSR;
                break;
            }

            case ARM_SFT_ROR_REG:
            {
                st = ShiftType::ST_ROR;
                break;
            }

            case ARM_SFT_RRX_REG:
            {
                st = ShiftType::ST_RRX;
                break;
            }

            default:
                break;
            }
        }

        return Operand2(reg, st, op.shift.value);
    }

    case ARM_OP_IMM:
    {
        return op.imm;
    }

    default:
        break;
    }

    UNREACHABLE("No proper method to translate Capstone operand to JIT's Operand2!");
}

struct arm_instruction_visitor
{
    address         current_pc;
    address         original_pc;

    bool            t_reg;

    std::uint8_t    *code;
    csh             handler;
    cs_insn         *insn;

    std::uint16_t op_counter = 0;

    void init()
    {
        cs_open(CS_ARCH_ARM, t_reg ? CS_MODE_THUMB : CS_MODE_ARM, &handler);

        // Add more details to examine instruction
        cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON)
    }

    ARMReg get_next_reg_from_cs(cs_arm *arm)
    {
        return cs_arm_op_to_reg(arm->operands[op_counter++]);
    }

    Operand2 get_next_op_from_cs(cs_arm *arm)
    {
        return cs_arm_op_to_operand2(arm->operands[op_counter++]);
    }

    void recompile_single_instruction(arm_block_of_code &block)
    {
        auto count = cs_disasm(handle, code, t_reg ? 2 : 4, current_pc, 1,
            &insn);

        cs_detail *detail = insn->detail;
        cs_arm *arm_detail = &(detail->arm);

        op_counter = 0;

        switch (insn->id)
        {
        case ARM_INS_MOV: 
        {
            block->gen_arm32_mov(get_next_reg_from_cs(arm), get_next_op_from_cs(arm));
            break;
        }

        case ARM_INS_MVN:
        {
            block->gen_arm32_mvn(get_next_reg_from_cs(arm), get_next_op_from_cs(arm));
            break;
        }

        case ARM_INS_ADD: 
        {
            block->gen_arm32_add(get_next_reg_from_cs(arm), get_next_reg_from_cs(arm),
                get_next_reg_from_cs(arm));

            break;
        }

        case ARM_INS_TST:
        {
            block->gen_arm32_tst(get_next_reg_from_cs(arm), get_next_op_from_cs(arm));
            break;
        }

        default:
        {
        }
        }

        current_pc += t_reg ? 2 : 4;
    }

    explicit arm_instruction_visitor(std::uint8_t *code, address pc)
        : code(code), original_pc(pc & 1 ? (pc - 1) : pc), current_pc(original_pc)
    {
        t_reg = (pc & 1) ? true : false;
        init();
    }

    arm_block_of_code   recompile(address addr)
    {

    }
}

// Mapped registers:
// r0-r9 used as default on ARM
// r11 for stack pointer
// r10 for JIT information pointer
// cpsr is reused

struct arm_block_of_code : public ARMXCodeBlock
{
    void                    *data;
    location_descriptor      descriptor;

    std::uint32_t            orginal_size;
    jit_callback             *callback;

    arm_instruction_visitor  *visitor;

    std::uint32_t            cycles;

    void ARMABI_call_function(void *func)
    {
        PUSH(5, ARMReg::R0, ARMReg::R1, ARMReg::R2, ARMRReg::R3, ARMReg::R_LR);
        MOVI2R(ARMReg::R14, reinterpret_cast<u32>(func));
        BL(ARMReg::R14);
        POP(ARMReg::R0, ARMReg::R1, ARMReg::R2, ARMRReg::R3, ARMReg::R_LR);
    }

    ARMReg remap_arm_reg(ARMReg reg)
    {
        switch (reg)
        {
        case ARMReg::R_SP: case ARMReg::R13:
        {
            return ARMReg::R10;
        }

        default:
            break;
        }

        return reg;
    }

    Operand2 remap_operand2(Operand2 op)
    {
        if (op.GetType() == OpType::TYPE_REG)
        {
            ARMReg reg = op.Rm();

            switch (reg)
            {
            case ARMReg::R13:
            {
                return ARMReg::R10;
            }

            case ARMReg::R15:
            {
                return visitor->current_pc;
            }

            default:
                break;
            }
        }

        return op;
    }

    void gen_arm32_mov(ARMReg reg, Operand2 op)
    {
        MOV(reg, remap_operand2(op));
    }

    void gen_arm32_mvn(ARMReg reg, Operand2 op)
    {
        MVN(reg, remap_operand2(op));
    }

    void gen_arm32_tst(ARMReg reg, Operand2 op)
    {
        (void);
    }

    void gen_arm32_teq(ARMReg reg, Operand2 op)
    {
        (void);
    }

    void gen_arm32_add(ARMReg reg1, ARMReg reg2, Operand2 op)
    {
        if (reg2 == ARMReg::R15) 
        {
            if (op.GetType() == OpType::TYPE_IMM)
            {
                // Add them right away, and move this value to destination
                // register
                std::uint32_t val = visitor->current_pc + 8 + op.Imm12() & 0b001111111111;
                MOV(reg1, val);
            }
            else
            {
                // ERROR
            }
        }

        ADD(remap_arm_reg(reg1), remap_arm_reg(reg2), op);
    }
    
    void gen_arm32_sub(ARMReg reg1, ARMReg reg2, Operand2 op)
    {
        if (reg2 == ARMReg::R15)
        {
            if (op.GetType() == OpType::TYPE_IMM)
            {
                // Add them right away, and move this value to destination
                // register
                std::uint32_t val = visitor->current_pc + 8 - op.Imm12() & 0b001111111111;
                MOV(reg1, val);
            }
            else
            {
                // ERROR
            }
        }

        SUB(remap_arm_reg(reg1), remap_arm_reg(reg2), op);
    }

    void gen_arm32_cmp(ARMReg reg1, Operand2 op)
    {
        CMP(remap_arm_reg(reg1), remap_operand2(op));
    }

    void gen_arm32_cmn(ARMReg reg1, Operand2 op)
    {
        CMN(remap_arm_reg(reg1), remap_operand2(op));
    }

    // R15 can't be used
    void gen_arm32_mul(ARMReg reg1, ARMReg reg2, ARMReg reg3)
    {
        MUL(remap_arm_reg(reg1), remap_arm_reg(reg2), remap_arm_reg(reg3));
    }
    
    // TODO: gen MLA + MLS
    void gen_arm32_umull(ARMReg reg1, ARMReg reg2, ARMReg reg3, ARMReg reg4)
    {
        UMULL(remap_arm_reg(reg1), remap_arm_reg(reg2), remap_arm_reg(reg3)
            , remap_arm_reg(reg4));
    }

    void gen_arm32_umulal(ARMReg reg1, ARMReg reg2, ARMReg reg3, ARMReg reg4)
    {
        UMLAL(remap_arm_reg(reg1), remap_arm_reg(reg2), remap_arm_reg(reg3)
            , remap_arm_reg(reg4));
    }

    void gen_arm32_smull(ARMReg reg1, ARMReg reg2, ARMReg reg3, ARMReg reg4)
    {
        SMULL(remap_arm_reg(reg1), remap_arm_reg(reg2), remap_arm_reg(reg3)
            , remap_arm_reg(reg4));
    }    
    
    void gen_arm32_smlal(ARMReg reg1, ARMReg reg2, ARMReg reg3, ARMReg reg4)
    {
        SMLAL(remap_arm_reg(reg1), remap_arm_reg(reg2), remap_arm_reg(reg3)
            , remap_arm_reg(reg4));
    }

    void gen_arm32_str(ARMReg reg1, ARMReg reg2, Operand2 base, bool subtract = false)
    {
        // Preserves these registers
        PUSH(2, ARMReg::R0, ARMReg::R1);

        // Move register 2 to reg4
        MOV(ARMReg::R0, remap_operand2(reg2));
        MOV(ARMReg::R1, remap_operand2(base));

        // Next, add them with base. The value will stay in R4
        if (subtract)
        {
            SUB(ARMReg::R1, ARMReg::R0, ARMReg::R1);
        }
        else
        {
            ADD(ARMReg::R1, ARMReg::R0, ARMReg::R1);
        }

        // Nice, we now got the address in R1, let's move the userdata to r0,
        // function pointer in r14 and branch.
        MOVI2R(ARMReg::R0, reinterpret_cast<u32>(callback->userdata));
        MOVI2R(ARMReg::R14, reinterpret_cast<u32>(callback->read_mem32));

        BL(ARMReg::R14);

        // Finally, read it back
        POP(2, ARMReg::R0, ARMReg::R1);
    }

    void gen_arm32_ldr(ARMReg reg1, ARMReg reg2, Operand2 op, bool subtract = false)
    {
        // The readed value will be returned in r0 anyway, so if the destination is r0,
        // that means we trash it and don't push it
        if (remap_arm_reg(reg1) != ARMReg::R0)
        {
            // Preserves these registers
            PUSH(2, ARMReg::R0, ARMReg::R1);
        }
        else
        {
            PUSH(1, ARMReg::R1);
        }

        // Move register 2 to reg4
        MOV(ARMReg::R0, remap_operand2(reg2));
        MOV(ARMReg::R1, remap_operand2(base));

        // Next, add them with base. The value will stay in R4
        if (subtract)
        {
            SUB(ARMReg::R1, ARMReg::R0, ARMReg::R1);
        }
        else
        {
            ADD(ARMReg::R1, ARMReg::R0, ARMReg::R1);
        }

        // Nice, we now got the address in R1, let's move the userdata to r0,
        // function pointer in r14 and branch.
        MOVI2R(ARMReg::R0, reinterpret_cast<u32>(callback->userdata));
        MOVI2R(ARMReg::R14, reinterpret_cast<u32>(callback->read_mem32));

        BL(ARMReg::R14);

        // We got the value in r0!
        
        // If the destination reg is already r0, we don't have to do anything
        if (reg1 != ARMReg::R0)
        {
            MOV(remap_arm_reg(reg1), ARMReg::R0);

            // Finally, read it back
            POP(1, ARMReg::R1);
        }
        else
        {
            // Finally, read it back
            POP(2, ARMReg::R0, ARMReg::R1);
        }
    }
};

struct jit
{
private:
    std::unordered_map<address, arm_block_of_code> 
        cache_blocks;

public:
    jit_callback            callback;

    std::uint32_t           regs[16];
    std::uint32_t           cpsr;
    std::uint32_t           fiq[8];
    std::uint32_t           svc[3];
    std::uint32_t           abt[3];
    std::uint32_t           irq[3];
    std::uint32_t           und[3];

    void execute(const std::uint32_t insts);
    bool stop();
};

}