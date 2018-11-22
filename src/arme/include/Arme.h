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

struct arm_instruction_visitor
{
    address         current_pc;
    address         original_pc;

    bool            t_reg;

    std::uint8_t    *code;
    csh             handler;
    cs_insn         *insn;

    std::uint16_t op_counter = 0;
    bool should_break = true;

    void init();

    ARMReg get_next_reg_from_cs(cs_arm *arm);
    Operand2 get_next_op_from_cs(cs_arm *arm);

    void recompile_single_instruction(arm_block_of_code &block);
    arm_block_of_code recompile(address addr);
    
    explicit arm_instruction_visitor(std::uint8_t *code, address pc);

    void set_code(std::uint8_t *new_code)
    {
        code = new_code;   
    }

    void set_pc(address pc)
    {
        current_pc = original_pc = (pc & 1) ? pc - 1 : pc;
        t_reg = (pc & 1) ? true : false;
    }
}

struct jit_state 
{
    std::uint32_t cycles_to_run;
    std::uint32_t cycles_left;
}

struct jit_state_information
{
    std::size_t offset_cycles_to_run;
    std::size_t offset_cycles_left;

    explicit jit_state_information(jit_state &state)
        : offset_cycles_to_run(offsetof(state, cycles_to_run)),
          offset_cycles_left(offsetof(state, offset_cycles_left))
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

    jit_state_information     jsi;

    void ARMABI_call_function(void *func);
    void ARMABI_save_all_registers();

    ARMReg remap_arm_reg(ARMReg reg);

    Operand2 remap_operand2(Operand2 op);
    
    void gen_arm32_mov(ARMReg reg, Operand2 op);
    void gen_arm32_mvn(ARMReg reg, Operand2 op);
    void gen_arm32_tst(ARMReg reg, Operand2 op);
    void gen_arm32_teq(ARMReg reg, Operand2 op);
    void gen_arm32_add(ARMReg reg1, ARMReg reg2, Operand2 op);
    void gen_arm32_sub(ARMReg reg1, ARMReg reg2, Operand2 op);
    void gen_arm32_cmp(ARMReg reg1, Operand2 op);
    void gen_arm32_cmn(ARMReg reg1, Operand2 op);

    // R15 can't be used
    void gen_arm32_mul(ARMReg reg1, ARMReg reg2, ARMReg reg3);

    // TODO: gen MLA + MLS
    void gen_arm32_umull(ARMReg reg1, ARMReg reg2, ARMReg reg3, ARMReg reg4);
    void gen_arm32_umulal(ARMReg reg1, ARMReg reg2, ARMReg reg3, ARMReg reg);
    void gen_arm32_smull(ARMReg reg1, ARMReg reg2, ARMReg reg3, ARMReg reg4);
    void gen_arm32_smlal(ARMReg reg1, ARMReg reg2, ARMReg reg3, ARMReg reg4);

    void gen_arm32_str(ARMReg reg1, ARMReg reg2, Operand2 base, bool subtract = false);
    void gen_arm32_ldr(ARMReg reg1, ARMReg reg2, Operand2 op, bool subtract = false);
    
    void gen_arm32_strb(ARMReg reg1, ARMReg reg2, Operand2 base, bool subtract = false);
    void gen_arm32_ldrb(ARMReg reg1, ARMReg reg2, Operand2 op, bool subtract = false);
    
    void gen_arm32_strh(ARMReg reg1, ARMReg reg2, Operand2 base, bool subtract = false);
    void gen_arm32_ldrh(ARMReg reg1, ARMReg reg2, Operand2 base, bool subtract = false);

    void gen_memory_write(void *func, ARMReg reg1, ARMReg reg2, Operand2 base, bool subtract = false);
    void gen_memory_read(void *func, ARMReg reg1, ARMReg reg2, Operand2 base, bool subtract = false);
    
    void gen_run_code()
    {

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