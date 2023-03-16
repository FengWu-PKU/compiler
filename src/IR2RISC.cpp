//
// Created by sakura on 2022/9/26.
//

#include "IR2RISC.h"
#include <cassert>
#include <iostream>
#include <memory>
#include<string.h>

using namespace std;

void IR2RISC::toRISC() {
    // 解析字符串 str, 得到 Koopa IR 程序
    koopa_program_t program;
    koopa_error_code_t ret = koopa_parse_from_string(str, &program);
    assert(ret == KOOPA_EC_SUCCESS);  // 确保解析时没有出错
    // 创建一个 raw program builder, 用来构建 raw program
    koopa_raw_program_builder_t builder = koopa_new_raw_program_builder();
    // 将 Koopa IR 程序转换为 raw program
    koopa_raw_program_t raw = koopa_build_raw_program(builder, program);
    // 释放 Koopa IR 程序占用的内存
    koopa_delete_program(program);
    // 处理 raw program
    // 遍历函数
    Visit(raw);

    // 处理完成, 释放 raw program builder 占用的内存
    // 注意, raw program 中所有的指针指向的内存均为 raw program builder 的内存
    // 所以不要在 raw program 处理完毕之前释放 builder
    koopa_delete_raw_program_builder(builder);
}


// 访问 raw program
void IR2RISC::Visit(const koopa_raw_program_t &program) {
    // 执行一些其他的必要操作
    // 访问所有全局变量
    cout<<" .data"<<endl;
    Visit(program.values);
    cout<<endl;
    // 访问所有函数
    cout<<" .text"<<endl;
    Visit(program.funcs);
}

// 访问 raw slice
void IR2RISC::Visit(const koopa_raw_slice_t &slice) {
    for (size_t i = 0; i < slice.len; ++i) {
        auto ptr = slice.buffer[i];
        // 根据 slice 的 kind 决定将 ptr 视作何种元素
        switch (slice.kind) {
            case KOOPA_RSIK_FUNCTION:
                // 访问函数
                Visit(reinterpret_cast<koopa_raw_function_t>(ptr));
                break;
            case KOOPA_RSIK_BASIC_BLOCK:
                // 访问基本块
                Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr));
                break;
            case KOOPA_RSIK_VALUE:
                // 访问指令
                Visit(reinterpret_cast<koopa_raw_value_t>(ptr));
                break;
            default:
                // 我们暂时不会遇到其他内容, 于是不对其做任何处理
                assert(false);
        }
    }
}

// 访问函数
void IR2RISC::Visit(const koopa_raw_function_t &func) {
    const koopa_raw_slice_t bbs=func->bbs;
    if(bbs.len==0) {  // 仅是函数声明，跳过
        return;
    }
    cout<<" .globl "<<(func->name)+1<<endl;
    cout<<(func->name)+1<<":"<<endl;
    cout<<" # prologue"<<endl;
    int cnt=0; // 记录需要为多少个变量分配栈空间
    hascall=0;
    int paramnum=0;
    for(size_t i=0; i<bbs.len;++i) {
        const koopa_raw_basic_block_t oneblock=reinterpret_cast<koopa_raw_basic_block_t>(bbs.buffer[i]);
        const koopa_raw_slice_t insts=oneblock->insts;
        for(size_t j =0; j<insts.len; ++j) {
            const koopa_raw_value_t inst=reinterpret_cast<koopa_raw_value_t>(insts.buffer[j]);
            auto tag=inst->ty->tag;  // ty.tag仅仅是返回值类型，x=f(2)的ty.tag是Int
            if(tag!=KOOPA_RTT_UNIT) {  // 该指令存在返回值
                if(inst->kind.tag==KOOPA_RVT_ALLOC) {
                    cnt += getSize(inst->ty);
                }else {
                    cnt+=4;
                }
            }
            if(inst->kind.tag==KOOPA_RVT_CALL) {
                hascall=1;
                koopa_raw_call_t call=inst->kind.data.call;
                if(paramnum<call.args.len) {
                    paramnum=call.args.len;
                }
            }
        }
    }
    if(paramnum-8<0) paramnum=0;  // 需要在栈上分配的参数个数
    stack_size=cnt+(hascall+paramnum)*4;
    // 对齐16
    for(int i=1;;++i) {
        if((16*i)>=stack_size) {
            stack_size=16*i;
            break;
        }
    }
//    assert(stack_size<=2047);
//    assert(stack_size>=-2048);
    var_pos= paramnum*4;  // 函数内临时变量在栈中的起点

    // 将形参加入stack_map,负数表示在寄存器中
    int tmp=0;  // 超过8的参数在调用者栈中的位置
    const koopa_raw_slice_t params=func->params;
    for(size_t i=0;i<params.len;i++) {
        if(i<8) {
            stack_map[(void*)params.buffer[i]]=-i;
        }else {
            stack_map[(void*)params.buffer[i]]=tmp+stack_size;  // 注意这里加了stack_size，是调用者的栈帧
            tmp+=4;
        }
    }
    if(stack_size<=2047)
        cout<<" addi sp, sp, -"<<stack_size<<endl;
    else {
        int s=stack_size;
        cout<<" li t5, 0"<<endl;
        while(s>2047) {
            cout<<" addi t5, t5, 2047"<<endl;
            s-=2047;
        }
        cout<<" addi t5, t5, "<<s<<endl;
        cout<<" sub sp, sp, t5"<<endl;
    }
    if(hascall) {
        // ra在当前栈帧的底部
//        cout<<" sw ra, "<<stack_size-4<<"(sp)"<<endl;
        stackOpt(stackOpt_type(SW), string("ra"), stack_size-4);
    }
    cout<<" # prologue end"<<endl;
    // 访问所有基本块
    Visit(func->bbs);
    cout<<endl;
}

// 访问基本块
void IR2RISC::Visit(const koopa_raw_basic_block_t &bb) {
    // 执行一些其他的必要操作
    // 输出label
    string name=bb->name;
    name=name.substr(1, name.length()-1);
    cout<<name<<':'<<endl;
    // 访问所有指令
    Visit(bb->insts);
}

// 访问指令
void IR2RISC::Visit(const koopa_raw_value_t &value) {
    // 根据指令类型判断后续需要如何访问
    const auto &kind = value->kind;
    int alloc_size=-1;  // 仅local alloc 用到
    switch (kind.tag) {
        case KOOPA_RVT_RETURN:
            // 访问 return 指令
            Visit(kind.data.ret);
            break;
        case KOOPA_RVT_INTEGER:
            // 访问 integer 指令
            Visit(kind.data.integer);
            break;
        case KOOPA_RVT_BINARY:
            Visit(kind.data.binary,value);
            break;
        case KOOPA_RVT_ALLOC:
            // 向stack_map里添加条目
            alloc_size= getSize(value->ty);
            stack_map[(void*)value]=var_pos;
            // value->ty.tag一定是KOOPA_RTT_POINTER
            if(value->ty->data.pointer.base->tag==KOOPA_RTT_ARRAY) {  // alloc 一个数组
                arr_map[(void*)value]=var_pos;
            }
            var_pos+=alloc_size;
            break;
        case KOOPA_RVT_GLOBAL_ALLOC:
            Visit(kind.data.global_alloc,value);
            break;
        case KOOPA_RVT_STORE:
            Visit(kind.data.store,value);
            break;
        case KOOPA_RVT_LOAD:
            Visit(kind.data.load,value);
            break;
        case KOOPA_RVT_BRANCH:
            Visit(kind.data.branch, value);
            break;
        case KOOPA_RVT_JUMP:
            Visit(kind.data.jump, value);
            break;
        case KOOPA_RVT_CALL:
            Visit(kind.data.call, value);
            break;
        case KOOPA_RVT_GET_ELEM_PTR:
            Visit(kind.data.get_elem_ptr,value);
            break;
        case KOOPA_RVT_GET_PTR:
            Visit(kind.data.get_ptr,value);
            break;
        default:
            // 其他类型暂时遇不到
            assert(false);
    }
}

// 访问return指令
void IR2RISC::Visit(const koopa_raw_return_t &ret) {
    if(ret.value) {  // has return value
        switch ((ret.value->kind).tag) {
            case KOOPA_RVT_INTEGER:
                cout<<" li a0, "<<ret.value->kind.data.integer.value<<endl;
                break;
            default:
//                cout<<" lw a0, "<<stack_map[(void*)ret.value]<<"(sp)"<<endl;
                stackOpt(stackOpt_type(LW), string("a0"), stack_map[(void*)ret.value]);
                break;
        }
    }
    cout<<" # epilogue"<<endl;
    if(hascall) {
//        cout<<" lw ra, "<<stack_size-4<<"(sp)"<<endl;
        stackOpt(stackOpt_type(LW), string("ra"), stack_size-4);
    }
    if(stack_size<=2047)
        cout<<" addi sp, sp, "<<stack_size<<endl;
    else {
        int s=stack_size;
        cout<<" li t5, 0"<<endl;
        while(s>2047) {
            cout<<" addi t5, t5, 2047"<<endl;
            s-=2047;
        }
        cout<<" addi t5, t5, "<<s<<endl;
        cout<<" add sp, sp, t5"<<endl;
    }
    cout<<" ret"<<endl;
}

// 访问int
void IR2RISC::Visit(const koopa_raw_integer_t &Int) {
    cout<<Int.value;
}

// 访问二元操作
// inst_ptr是指令的地址，且stack_map中的Key也是指令的地址，binary.l(r)hs也是指令的地址
// 使用t0, t1,结果暂存t0，会产生新的变量
void IR2RISC::Visit(const koopa_raw_binary_t &binary,const koopa_raw_value_t &inst_ptr) {
    // 左边的操作数
    int li, left_pos;
    switch ((binary.lhs->kind).tag) {
        case KOOPA_RVT_INTEGER:
            li=binary.lhs->kind.data.integer.value;
            cout<<" li t0, "<<li<<endl;
            break;
        default:
            left_pos=stack_map[(void*)binary.lhs];
//            cout<<" lw t0, "<<left_pos<<"(sp)"<<endl;
            stackOpt(stackOpt_type(LW), string("t0"), left_pos);
            break;
    }
    // 右边的操作数
    int ri, right_pos;
    switch ((binary.rhs->kind).tag) {
        case KOOPA_RVT_INTEGER:
            ri=binary.rhs->kind.data.integer.value;
            cout<<" li t1, "<<ri<<endl;
            break;
        default:
            right_pos=stack_map[(void*)binary.rhs];
//            cout<<" lw t1, "<<right_pos<<"(sp)"<<endl;
            stackOpt(stackOpt_type(LW), string("t1"), right_pos);
            break;
    }
    stack_map[(void*)inst_ptr]=var_pos;  // 为当前%临时变量分配的栈位置
    var_pos+=4;

    const auto &op=binary.op;
    switch (op) {
        case KOOPA_RBO_ADD:
            cout<<" "<<"add t0, t0, t1"<<endl;
            break;
        case KOOPA_RBO_SUB:
            cout<<" "<<"sub t0, t0, t1"<<endl;
            break;
        case KOOPA_RBO_EQ:
            cout<<" "<<"xor t0, t0, t1"<<endl;
            cout<<" "<<"seqz t0, t0"<<endl;
            break;
        case KOOPA_RBO_MUL:
            cout<<" "<<"mul t0, t0, t1"<<endl;
            break;
        case KOOPA_RBO_DIV:
            cout<<" "<<"div t0, t0, t1"<<endl;
            break;
        case KOOPA_RBO_MOD:
            cout<<" "<<"rem t0, t0, t1"<<endl;
            break;
        case KOOPA_RBO_LT:
            cout<<" "<<"slt t0, t0, t1"<<endl;
            break;
        case KOOPA_RBO_GT:
            cout<<" "<<"slt t0, t1, t0"<<endl;
            break;
        case KOOPA_RBO_LE:
            cout<<" "<<"slt t0, t1, t0"<<endl;
            cout<<" "<<"xori t0, t0, 1"<<endl;
            break;
        case KOOPA_RBO_GE:
            cout<<" "<<"slt t0, t0, t1"<<endl;
            cout<<" "<<"xori t0, t0, 1"<<endl;
            break;
        case KOOPA_RBO_OR:
            cout<<" "<<"or t0, t0, t1"<<endl;
            break;
        case KOOPA_RBO_AND:
            cout<<" "<<"and t0, t0, t1"<<endl;
            break;
        case KOOPA_RBO_NOT_EQ:
            cout<<" "<<"xor t0, t0, t1"<<endl;
            cout<<" "<<"snez t0, t0"<<endl;
            break;
    }
//    cout<<" sw t0, "<<stack_map[(void*)inst_ptr]<<"(sp)"<<endl;
    stackOpt(stackOpt_type(SW), string("t0"), stack_map[(void*)inst_ptr]);
}


/**
 * 访问store
 * @param store
 * @param inst_ptr
 * 不会产生新的变量，所以inst_ptr没有用到
 */
void IR2RISC::Visit(const koopa_raw_store_t &store, const koopa_raw_value_t &inst_ptr) {
    koopa_raw_value_t value=store.value;
    koopa_raw_value_t dest=store.dest;
    // 把要保存的东西放入t0， switch区分待保存值的类型，内层if区分value的来源
    int value_pos;
    switch (value->ty->tag) {
        case KOOPA_RTT_INT32:
            if(value->kind.tag==KOOPA_RVT_INTEGER) {  // 直接就是立即数
                cout<<" li t0"<<", "<<store.value->kind.data.integer.value<<endl;
            }else if(value->kind.tag!=KOOPA_RVT_GLOBAL_ALLOC) {  // 局部变量
                int value_pos=stack_map[(void*)store.value];
                if(value_pos<=0) {
                    cout<<" mv t0, a"<<-value_pos<<endl;
                }else {
    //                cout<<" lw t0, "<<value_pos<<"(sp)"<<endl;
                    stackOpt(stackOpt_type(LW),string("t0"), value_pos);
                }

            }else {  // 全局变量
                string name=global_var_name[(void*)value];
                cout<<" la t0, "<<name<<endl;
                cout<<" lw t0, 0(t0)"<<endl;
            }
            break;
        case KOOPA_RTT_POINTER:  // 要保存的是指针，则它一定在栈上有位置，把栈地址存入t0
            value_pos=stack_map[(void*)value];
            if(value_pos<=0) {  // 寄存器中的形参
                cout<<" mv t0, a"<<-value_pos<<endl;
            }else {  // 栈中的位置
                cout<<" addi t0, sp, "<<value_pos<<endl;
            }
            break;
        default:
            cerr<<"store type err"<<endl;
            assert(false);
    }
    // 处理dest
    if(stack_map.find((void*)dest)==stack_map.end()) {  // dest is global
        string dest_name=global_var_name[(void*)dest];
        cout<<" la t1, "<<dest_name<<endl;
        cout<<" sw t0, 0(t1)"<<endl;
    }else {  // dest is local
        int dest_pos=stack_map[(void*)dest];
        if(dest->kind.tag==KOOPA_RVT_GET_ELEM_PTR||dest->kind.tag==KOOPA_RVT_GET_PTR) {
//            cout<<" lw t1, "<<dest_pos<<"(sp)"<<endl;
            stackOpt(stackOpt_type(LW),string("t1"),dest_pos);
            cout<<" sw t0, 0(t1)"<<endl;
        }else {
//            cout<<" sw t0"<<", "<<dest_pos<<"(sp)"<<endl;
            stackOpt(stackOpt_type(SW),string("t0"), dest_pos);
        }
    }
    return;
//    switch (store.value->kind.tag) {
//        case KOOPA_RVT_INTEGER:
//            cout<<" li t0"<<", "<<store.value->kind.data.integer.value<<endl;
//            break;
//        default:
//            // store.value一定是局部的临时变量，koopaIR形式保证的，也许？？？
//            int value_pos=stack_map[(void*)store.value];
//            if(value_pos<=0) {
//                cout<<" mv t0"<<", "<<"a"<<-value_pos<<endl;
//            }else {
//                cout<<" lw t0"<<", "<<value_pos<<"(sp)"<<endl;
//            }
//            break;
//    }
//    if(stack_map.find((void*)store.dest)==stack_map.end()) {  // store.dest是全局变量
//        string name=global_var_name[(void*)store.dest];
//        cout<<" la t1, "<<name<<endl;
//        cout<<" sw t0, 0(t1)"<<endl;
//    }else {
//        if(store.value->ty->tag==) {  // value的值是指针，也即value是指针的指针
//            int ptr_pos=stack_map[(void*)store.dest];
//            cout<<" lw t1, "<<ptr_pos<<"(sp)"<<endl;
//            cout<<" sw t0, 0(t1)"<<endl;
//        }else {
//            int des_pos=stack_map[(void*)store.dest];
//            cout<<" sw t0"<<", "<<des_pos<<"(sp)"<<endl;
//        }
//    }
}

/**
 *
 * @param load
 * @param inst_ptr
 * 只使用t0一个寄存器，会产生新的变量，需要分配栈空间
 */
void IR2RISC::Visit(const koopa_raw_load_t &load, const koopa_raw_value_t &inst_ptr) {
    if(stack_map.find((void*)load.src)==stack_map.end()) {  // load global variable
        string varname=global_var_name[(void*)load.src];
        cout<<" la t0, "<<varname<<endl;
        cout<<" lw t0, 0(t0)"<<endl;
    }else {  // load local variable
        int src_pos=stack_map[(void*)load.src];
//        cout<<" lw t0, "<<src_pos<<"(sp)"<<endl;
        stackOpt(stackOpt_type(LW),string("t0"), src_pos);
        if(load.src->kind.tag==KOOPA_RVT_GET_ELEM_PTR||load.src->kind.tag==KOOPA_RVT_GET_PTR) {  // getelemptr, getptr存在栈中的是地址，需要取出来
            cout<<" lw t0, 0(t0)"<<endl;
        }
    }
    // 分配新变量
    stack_map[(void*)inst_ptr]=var_pos;
    var_pos+=4;
//    cout<<" sw t0, "<<stack_map[(void*)inst_ptr]<<"(sp)"<<endl;
    stackOpt(stackOpt_type(SW),string("t0"), stack_map[(void*)inst_ptr]);
}

void IR2RISC::Visit(const koopa_raw_branch_t &branch, const koopa_raw_value_t &inst_ptr) {
    int cond_num;
    int cond_pos;
    bool isNum=false;
    switch(branch.cond->kind.tag) {
        case KOOPA_RVT_INTEGER:
            cond_num=branch.cond->kind.data.integer.value;
            isNum=true;
            break;
        default:
            cond_pos=stack_map[(void*)branch.cond];
            break;
    }
    string true_bb=branch.true_bb->name;
    true_bb=true_bb.substr(1,true_bb.length()-1);  // 去掉%号
    if(isNum) {
        cout<<" li t0, "<<cond_num<<endl;
        cout<<" bnez t0, "<<true_bb<<endl;
    }else {
//        cout<<" lw t0, "<<cond_pos<<"(sp)"<<endl;
        stackOpt(stackOpt_type(LW),string("t0"),cond_pos);
        cout<<" bnez t0, "<<true_bb<<endl;
    }
    string false_bb=branch.false_bb->name;
    false_bb=false_bb.substr(1,false_bb.length()-1);
    cout<<" j "<<false_bb<<endl;
}

void IR2RISC::Visit(const koopa_raw_jump_t &jump, const koopa_raw_value_t &inst_ptr) {
    string target=jump.target->name;
    target=target.substr(1, target.length()-1);
    cout<<" j "<<target<<endl;
}


// 访问call指令
void IR2RISC::Visit(const koopa_raw_call_t &call, const koopa_raw_value_t &inst_ptr) {
    int paramnum=call.args.len;
    // call.args.kind=KOOPA_RISC_VALUE
    for(int i=0;i<paramnum&&i<8;i++) {
        koopa_raw_value_t cur=reinterpret_cast<koopa_raw_value_t>(call.args.buffer[i]);
        switch (cur->kind.tag) {
            case KOOPA_RVT_INTEGER:
                cout<<" li a"<<i<<", "<<cur->kind.data.integer.value<<endl;
                break;
            default:
                // 需要从栈中取出来
                int pos=stack_map[(void*)cur];
//                cout<<" lw a"<<i<<", "<<pos<<"(sp)"<<endl;
                stackOpt(stackOpt_type(LW),string("a"+to_string(i)),pos);
                break;
        }
    }
    param_pos=0;  // 每次call都是从0开始分配第9个参数...
    for(int i=8;i<paramnum;i++) {  // 为剩余参数在栈上分配空间
        koopa_raw_value_t cur=reinterpret_cast<koopa_raw_value_t>(call.args.buffer[i]);
        switch (cur->kind.tag) {
            case KOOPA_RVT_INTEGER:
                cout<<" li t0, "<<cur->kind.data.integer.value<<endl;
//                cout<<" sw t0, "<<param_pos<<"(sp)"<<endl;
                stackOpt(stackOpt_type(SW),string("t0"),param_pos);
                param_pos+=4;
                break;
            default:
                // 需要从栈中取出来
                int pos=stack_map[(void*)cur];
//                cout<<" lw t0, "<<pos<<"(sp)"<<endl;
                stackOpt(stackOpt_type(LW),string("t0"),pos);
//                cout<<" sw t0, "<<param_pos<<"(sp)"<<endl;
                stackOpt(stackOpt_type(SW),string("t0"),param_pos);
                param_pos+=4;
                break;
        }
    }
    cout<<" call "<<(call.callee->name)+1<<endl;
    if(inst_ptr->ty->tag!=KOOPA_RTT_UNIT) {  // 调用的函数不是void，保存a0
        stack_map[(void*)inst_ptr]=var_pos;
//        cout<<" sw a0, "<<var_pos<<"(sp)"<<endl;
        stackOpt(stackOpt_type(SW),string("a0"),var_pos);
        var_pos+=4;
    }
}
void IR2RISC::loadArr(const koopa_raw_slice_t &elems) {
    koopa_raw_value_t tmp=reinterpret_cast<koopa_raw_value_t>(elems.buffer[0]);
    if(tmp->kind.tag==KOOPA_RVT_INTEGER) {  // 最后一维
        for(size_t i=0;i<elems.len;i++) {
            koopa_raw_value_t value=reinterpret_cast<koopa_raw_value_t>(elems.buffer[i]);
            std::cout<<".word "<<value->kind.data.integer.value<<endl;
        }
    }else {
        for(size_t i=0;i<elems.len;i++) {
            koopa_raw_value_t value=reinterpret_cast<koopa_raw_value_t>(elems.buffer[i]);
            loadArr(value->kind.data.aggregate.elems);
        }
    }
    return;
}

// global alloc
void IR2RISC::Visit(const koopa_raw_global_alloc_t &global_alloc, const koopa_raw_value_t &inst_ptr) {
    string curname="var"+to_string(global_name_cnt++);
    cout<<" .globl "<<curname<<endl;
    global_var_name[(void*)inst_ptr]=curname;
    cout<<curname<<':'<<endl;
    const auto &init_kind=global_alloc.init->kind;
    const auto &init_ty=global_alloc.init->ty;
    int zeroinit_size=1;
    koopa_raw_slice_t elems;
    switch (init_kind.tag) {
        case KOOPA_RVT_ZERO_INIT:
            if(init_ty->tag==KOOPA_RTT_INT32) {
                zeroinit_size=4;
            }else if(init_ty->tag==KOOPA_RTT_ARRAY) {
                auto ty=init_ty;
                while(ty->tag==KOOPA_RTT_ARRAY) {
                    zeroinit_size*=ty->data.array.len;
                    ty=ty->data.array.base;
                }
                zeroinit_size*=4;
            }else {
                cerr<<"zeroinit kind wrong."<<endl;
                return;
            }
            cout<<" .zero "<<zeroinit_size<<endl;
            break;
        case KOOPA_RVT_INTEGER:
            cout<<" .word "<<init_kind.data.integer.value<<endl;
            break;
        case KOOPA_RVT_AGGREGATE:
            elems=init_kind.data.aggregate.elems;
            loadArr(elems);
            cout<<endl;
            break;
        default:
            cerr<<"global alloc initVal have undefined type"<<endl;
            assert(false);
    }
}


void IR2RISC::Visit(const koopa_raw_get_elem_ptr_t elemPtr, const koopa_raw_value_t &inst_ptr) {
    koopa_raw_value_t src=elemPtr.src;
    // 数组首地址存入t0
    if(stack_map.find((void*)src)==stack_map.end()) {
        string name=global_var_name[(void*)src];
        cout<<" la t0, "<<name<<endl;
    }else {
        int arrpos=stack_map[(void*)src];
        if(arr_map.find((void*)src)!=arr_map.end()) { // 从数组第一维中getelemptr
            cout<<" addi t0, sp, "<<arrpos<<endl;
        }else {  // 开头的地址存在了栈中
//            cout<<" lw t0, "<<arrpos<<"(sp)"<<endl;
            stackOpt(stackOpt_type(LW),string("t0"), arrpos);
        }

//        if(src->kind.tag==KOOPA_RVT_GET_ELEM_PTR||src->kind.tag==KOOPA_RVT_GET_PTR) {
//            cout<<" lw t0, "<<arrpos<<"(sp)"<<endl;  // 指针存在了栈里边
//        }else {
//            cout<<" addi t0, sp, "<<arrpos<<endl;
//        }
    }

    // 计算偏移量
    int Size= getSize(src->ty->data.pointer.base->data.array.base);
    cout<<" li t1, "<<Size<<endl;
    koopa_raw_value_t index=elemPtr.index;
    int indexpos=-1;
    switch (index->kind.tag) {
        case KOOPA_RVT_INTEGER:   // %ptr=getelemptr @arr, 1
            indexpos=index->kind.data.integer.value;
            cout<<" li t2, "<<indexpos<<endl;
            break;
        default:  // %ptr=getelemptr @arr, %2
            indexpos=stack_map[(void*)index];
//            cout<<" lw t2, "<<indexpos<<"(sp)"<<endl;
            stackOpt(stackOpt_type(LW),string("t2"),indexpos);
    }
    cout<<" mul t1, t1, t2"<<endl;  // 可以用移位指令优化，先不管了orz
    // 计算结果,存在t0
    cout<<" add t0, t0, t1"<<endl;  // 对应元素的地址
    stack_map[(void*)inst_ptr]=var_pos;
//    cout<<" sw t0, "<<var_pos<<"(sp)"<<endl;
    stackOpt(stackOpt_type(SW),string("t0"), var_pos);
    var_pos+=4;
}

void IR2RISC::Visit(const koopa_raw_get_ptr_t ptr, const koopa_raw_value_t &inst_ptr) {
    koopa_raw_value_t src=ptr.src;
    // 数组首地址存入t0
    if(stack_map.find((void*)src)==stack_map.end()) {
        string name=global_var_name[(void*)src];
        cout<<" la t0, "<<name<<endl;
    }else {
        int arrpos=stack_map[(void*)src];
//        cout<<" lw t0, "<<arrpos<<"(sp)"<<endl;  // 指针存在了栈里边
        stackOpt(stackOpt_type(LW),string("t0"),arrpos);

    }
    // 计算偏移量
    int Size= getSize(src->ty->data.pointer.base);  // 只能是4，ptr_int, ptr_arr
    cout<<" li t1, "<<Size<<endl;
    koopa_raw_value_t index=ptr.index;
    int indexpos=-1;
    switch (index->kind.tag) {
        case KOOPA_RVT_INTEGER:   // %ptr=getptr @arr, 1
            indexpos=index->kind.data.integer.value;
            cout<<" li t2, "<<indexpos<<endl;
            break;
        default:  // %ptr=getptr @arr, %2
            indexpos=stack_map[(void*)index];
//            cout<<" lw t2, "<<indexpos<<"(sp)"<<endl;
            stackOpt(stackOpt_type(LW),string("t2"),indexpos);
    }
    cout<<" mul t1, t1, t2"<<endl;  // 可以用移位指令优化，先不管了orz
    // 计算结果,存在t0
    cout<<" add t0, t0, t1"<<endl;
    stack_map[(void*)inst_ptr]=var_pos;
//    cout<<" sw t0, "<<var_pos<<"(sp)"<<endl;
    stackOpt(stackOpt_type(SW),string("t0"), var_pos);
    var_pos+=4;
}

int IR2RISC::getSize(const koopa_raw_type_t &ty) {
    if(ty->tag==KOOPA_RTT_INT32) {
        return 4;
    }else if(ty->tag==KOOPA_RTT_ARRAY) {
        int len=ty->data.array.len;
        int elem_size= getSize(ty->data.array.base);
        return len*elem_size;
    }else if(ty->tag==KOOPA_RTT_POINTER) {
        return getSize(ty->data.pointer.base);
//        return 4;
    }else if(ty->tag==KOOPA_RTT_FUNCTION) {
        return 4;
    }else {
        cerr<<"getsize can't deal with other type"<<endl;
        assert(false);
    }
}

void IR2RISC::stackOpt(stackOpt_type type, string reg, int pos) {
    if(pos<=2047) {
        if(type==SW) {
            cout<<" sw "<<reg<<", "<<pos<<"(sp)"<<endl;
            return;
        }else if(type==LW) {
            cout<<" lw "<<reg<<", "<<pos<<"(sp)"<<endl;
            return;
        }
    }
    cout<<" li t5, 0"<<endl;
    while(pos>2047) {
        cout<<" addi t5, t5, 2047"<<endl;
        pos-=2047;
    }
    cout<<" addi t5, t5, "<<pos<<endl;
    cout<<" add t5, t5, sp"<<endl;
    if(type==SW) {
        cout<<" sw "<<reg<<", 0(t5)"<<endl;
    }else if(type==LW) {
        cout<<" lw "<<reg<<", 0(t5)"<<endl;
    }
    return;
}


