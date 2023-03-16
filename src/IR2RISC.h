//
// Created by sakura on 2022/9/26.
//

#ifndef COMPILER_IR2RISC_H
#define COMPILER_IR2RISC_H

#include "koopa.h"
#include <map>
#include <vector>
#include <cmath>
#include <string>

typedef enum {
    SW,
    LW,
} stackOpt_type;

class IR2RISC {
private:
    const char *str;
public:
    /*std::map<void*, const char*> reg_map;
    int reg_cnt=0;  // t0-t6(reg_cnt:0-6), a2-a7(reg_cnt:7-12) */
    const char* reg[15]={"t0", "t1", "t2", "t3", "t4",
    "t5", "t6", "a0","a1","a2", "a3", "a4", "a5", "a6", "a7"};
    int stack_size=0;  // 分配的总栈空间大小
    int hascall=0;  // 函数内有无call指令
    std::map<void*, int> stack_map;  // (指令地址，栈上位置)
    std::map<void*, int> arr_map;
    std::map<void*, std::string> global_var_name;  // (指令地址，变量名称)
    int global_name_cnt=0;
    int var_pos=0;  // 下一个分配的变量在栈上的偏移量
    int param_pos=0;  // 超过8的参数在调用者栈帧中的偏移量
    IR2RISC(const char *ir) {
        str=ir;  // char* 可直接用=进行深拷贝
    }
    void toRISC();
    void loadArr(const koopa_raw_slice_t &elems);
    int getSize(const koopa_raw_type_t &ty);
    void Visit(const koopa_raw_program_t &program);
    void Visit(const koopa_raw_slice_t &slice);
    void Visit(const koopa_raw_function_t &func);
    void Visit(const koopa_raw_basic_block_t &bb);
    void Visit(const koopa_raw_value_t &value);
    void Visit(const koopa_raw_return_t &ret);
    void Visit(const koopa_raw_integer_t &Int);
    void Visit(const koopa_raw_binary_t &binary, const koopa_raw_value_t &inst_ptr);
    void Visit(const koopa_raw_store_t &store, const koopa_raw_value_t &inst_ptr);
    void Visit(const koopa_raw_load_t &load, const koopa_raw_value_t &inst_ptr);
    void Visit(const koopa_raw_branch_t &branch, const koopa_raw_value_t &inst_ptr);
    void Visit(const koopa_raw_jump_t &jump, const koopa_raw_value_t &inst_ptr);
    void Visit(const koopa_raw_call_t &call, const koopa_raw_value_t &inst_ptr);
    void Visit(const koopa_raw_global_alloc_t &global_alloc, const koopa_raw_value_t &inst_ptr);
    void Visit(const koopa_raw_get_elem_ptr_t elemPtr, const koopa_raw_value_t &inst_ptr);
    void Visit(const koopa_raw_get_ptr_t ptr, const koopa_raw_value_t &inst_ptr);
    void stackOpt(stackOpt_type type, std::string reg, int pos);
};


#endif //COMPILER_IR2RISC_H
