#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <stdio.h>
#include <fstream>
#include <sstream>
#include "AST.h"
#include "IR2RISC.h"

using namespace std;

// 声明 lexer 的输入, 以及 parser 函数
// 为什么不引用 sysy.tab.hpp 呢? 因为首先里面没有 yyin 的定义
// 其次, 因为这个文件不是我们自己写的, 而是被 Bison 生成出来的
// 你的代码编辑器/IDE 很可能找不到这个文件, 然后会给你报错 (虽然编译不会出错)
// 看起来会很烦人, 于是干脆采用这种看起来 dirty 但实际很有效的手段
extern FILE *yyin;
extern int yyparse(unique_ptr<BaseAST> &ast);

int BaseAST::tmp_cnt=0;
int BaseAST::if_cnt=-1;  // 为了从0开始
int BaseAST::while_cnt=-1;
int BaseAST::result_cnt=0;
string BaseAST::func_name="";
symTable* BaseAST::top=new symTable();  // 全局符号表
symTable* BaseAST::global=top;
map<string, PTR> BaseAST::type_check=map<string, PTR>();

int main(int argc, const char *argv[]) {
    // 解析命令行参数. 测试脚本/评测平台要求你的编译器能接收如下参数:
    // compiler 模式 输入文件 -o 输出文件
    assert(argc == 5);
    auto mode = argv[1];
    auto input = argv[2];
    auto output = argv[4];

    // 打开输入文件, 并且指定 lexer 在解析的时候读取这个文件
    yyin = fopen(input, "r");
    assert(yyin);

    // 调用 parser 函数, parser 函数会进一步调用 lexer 解析输入文件的
    unique_ptr<BaseAST> ast;
    auto ret = yyparse(ast);
    assert(!ret);

    string Mode;
    Mode=mode;
    if (Mode.compare("-koopa")==0) {
        freopen(output,"w",stdout);
        ast->Dump();
    }
    else if(Mode.compare("-riscv")==0) {
        // 生成*.koopa的文件名
        string inputString;
        inputString=input;
        string irfile=inputString.substr(0,inputString.rfind("."))+".koopa";
        // 输出*.koopa
        freopen(irfile.c_str(),"w",stdout);
        ast->Dump();
        // 重定向至output
        ifstream ifile(irfile,std::ios::binary);
        stringstream ss;
        ss<<ifile.rdbuf();
        string data=ss.str();
        freopen(output,"w",stdout);

        IR2RISC trans(data.c_str());
        trans.toRISC();
    }
    return 0;
}
