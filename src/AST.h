//
// Created by sakura on 2022/9/22.
//

#ifndef SYSY_MAKE_TEMPLATE_AST_H
#define SYSY_MAKE_TEMPLATE_AST_H
#include <iostream>
#include <vector>
#include <map>
#include "symTable.h"
#include <stdlib.h>
#include <memory>

void load_libfunc();

typedef enum {
    PTR_INT,
    PTR_PTR,
    PTR_ARR,
} ptr_type;

struct PTR {
    ptr_type pt;
    PTR* base;  // PTR_PTR
};

class BaseAST {
public:
    static int tmp_cnt;  // 用来临时变量计数，初始化为0
    static int if_cnt;  // 用来做if语句的label
    static int while_cnt;  // 用来做while语句的label，分开是方便debug
    static int result_cnt;  // 用来短路求值中res的计数
    static std::string func_name;  // 用来对label做前缀防止重复
    static symTable* top;  // 当前的符号表,初始化为全局符号表,为每个while添加一个待完成的break和continue
    static symTable* global;  // 全局符号表
    static std::map<std::string, PTR> type_check;


    virtual ~BaseAST()=default;
    virtual void Dump()  =0;
    virtual void Dump(var &res)  =0;  // 返回值int存到res里
    virtual void Dump(bool &hasret)  =0;  // 语句return
};

class CompUnitAST:public BaseAST {
public:
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> defs;
    void Dump()  override{
        load_libfunc();
        for(auto it=defs->begin();it!=defs->end();it++) {
            (*it)->Dump();
        }
    }
    void Dump(var &res)  override {}
    void Dump(bool &hasret)  override {}
};

class FuncFParamAST:public BaseAST {
public:
    std::string btype;
    std::string ident;
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> indexes;
    void Dump()  override {}
    void Dump(var &res)  override {  // res 开始传递进来i,标识第几个参数
        int id=std::get<int>(res);
        std::cout<<"@param"<<id<<": ";
        if(btype.compare("int")==0&&!indexes) {
            std::cout<<"i32";
            res=var(std::string("i32"));
        }else {
            std::cout<<'*';
            if(indexes->size()==0)  {
                std::cout<<"i32";
                res=var(std::string("*i32"));
            }
            else {
                std::string type="*";
                int dimNum=indexes->size();
                int* dim=new int[dimNum+1];
                int i=1;  // 暂时用来计数
                for(auto it=indexes->begin();it!=indexes->end();it++) {
                    var tmp;
                    (*it)->Dump(tmp);
                    if(tmp.index()!=0) {
                        std::cerr<<"arr index is not const numbers."<<std::endl;
                        return;
                    }
                    dim[i++]=std::get<int>(tmp);
                }
                for(int i=1;i<dimNum;i++) {
                    std::cout<<'[';
                    type+="[";
                }
                std::cout<<"[ i32, "<<dim[dimNum]<<']';
                type+="[ i32, "+std::to_string(dim[dimNum])+"]";
                for(int i=dimNum-1;i>=1;i--) {
                    std::cout<<", "<<dim[i]<<']';
                    type+=", "+std::to_string(dim[i])+"]";
                }
                res=var(type);
            }
        }
    }
    void Dump(bool &hasret)  override {}
};

class FuncDefAST:public BaseAST {
public:
    std::string func_type;
    std::string ident;
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> params;
    std::unique_ptr<BaseAST> block;
    void Dump()  override {
        func_name=ident;
        std::string funcname=ident+"()";
        if(top->contain(funcname)) {
            std::cerr<<"function name duplicates!"<<std::endl;
            return;
        }
        top->insert(funcname,var(func_type));  // pair(f(), int)

        symTable* saved=top;
        top=new symTable(top);
        std::cout<<"fun @"<<ident<<"(";
        std::vector<std::string> types;
        int id=0;
        for(auto it=params->begin();it!=params->end();) {
            var tmp=var(id++);
            (*it)->Dump(tmp);
            types.push_back(std::get<std::string>(tmp));
            it++;
            if(it!=params->end()) std::cout<<", ";
        }
        std::cout<<")";
        if(func_type.compare("int")==0) {
            std::cout<<": i32 ";
        }
        std::cout<<"{"<<std::endl;
        std::cout<<"%"<<func_name<<"_entry:"<<std::endl;
        // 重新载入形参 @x->%x
        auto iter=types.begin();
        id=0;
        for(auto it=params->begin();it!=params->end();it++) {
            FuncFParamAST* cur=(FuncFParamAST*)((*it).get());
            std::string alloc="%"+cur->ident;
            std::string type=(*iter);
            PTR p;
            if(type[0]=='i') {
                p.pt=PTR_INT;
                type_check[cur->ident]=p;
            }else if(type[0]=='*') {
                p.pt=PTR_PTR;
                if(type[1]=='i') {
                    p.base=new PTR();
                    p.base->pt=PTR_INT;
                    type_check[cur->ident]=p;
                }else if(type[1]=='[') {
                    int i=1;
                    p.base=new PTR();
                    PTR* c=p.base;
                    while(type[i++]=='[') {
                        c->pt=PTR_ARR;
                        c->base=new PTR();
                        c=c->base;
                    }
                    c->pt=PTR_INT;
                    type_check[cur->ident]=p;
                }
            }

            std::cout<<"    "<<alloc<<" = alloc "<<type<<std::endl;
            iter++;
            std::cout<<"    store @param"<<id++<<", "<<alloc<<std::endl;
            top->insert(cur->ident,var(alloc));
            // 注意重新载入形参后，在形参原类型上添加了*

        }
        bool hasret=false;
        block->Dump(hasret);
        if(!hasret) {  // 函数中没有ret，对应void function
            std::cout<<"    ret"<<std::endl;
        }
        std::cout<<"}"<<std::endl;
        delete top;
        top=saved;
    }
    void Dump(var &res)  override {}
    void Dump(bool &hasret) override {}
};


class BlockAST:public BaseAST {
public:
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> blockitem;
    void Dump(bool &hasret)  override {
        for(auto it=blockitem->begin();it!=blockitem->end();it++) {
            if(hasret) break;  // 块中出现了ret ，就不再处理中剩下的指令
            (*it)->Dump(hasret);
        }
    }
    void Dump(var &res)  override {}
    void Dump()  override {}
};


class While_StmtAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> exp;
    std::unique_ptr<BaseAST> stmt;
    void Dump()  override {
        while_cnt++;
        std::string entry="%"+func_name+"_while_entry";
        entry+=std::to_string(while_cnt);
        std::string BTrue="%"+func_name+"_while_body";
        BTrue+=std::to_string(while_cnt);
        std::string BFalse="%"+func_name+"_while_end";
        BFalse+=std::to_string(while_cnt);
        symTable* saved=top;
        top=new symTable(top);
        top->insert(std::string("break"),var(BFalse));
        top->insert(std::string("continue"),var(entry));

        std::cout<<"    jump "<<entry<<std::endl;
        std::cout<<std::endl;
        std::cout<<entry<<':'<<std::endl;
        var tmp;
        exp->Dump(tmp);
        if(tmp.index()==1) {  // exp是表达式
            int operand=BaseAST::tmp_cnt-1;
            std::cout<<"    br %"<<operand<<", "<<BTrue<<", "<<BFalse<<std::endl;
        }else if(tmp.index()==0) {  // exp是常数值
            std::cout<<"    br "<<std::get<int>(tmp)<<", "<<BTrue<<", "<<BFalse<<std::endl;
        }
        std::cout<<std::endl;
        std::cout<<BTrue<<":"<<std::endl;
        bool ret=false;
        stmt->Dump(ret);
        if(!ret)  // body语句块中没有return才jump
            std::cout<<"    jump "<<entry<<std::endl;
        std::cout<<std::endl;
        std::cout<<BFalse<<":"<<std::endl;
        // 恢复top
        delete top;
        top=saved;
    }
    void Dump(var &res)  override {
        this->Dump();
    }
    void Dump(bool &hasret)  override {
        this->Dump();
    }
};

class LValAST:public BaseAST {
public:
    std::string ident;
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> indexes;  // exp
    LValAST() {
        indexes=std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>>(new std::vector<std::unique_ptr<BaseAST>>);
    }
    void Dump()  override{}
    void Dump(var &res)  override{
        var tmp=top->getValue(ident);
        if(indexes->size()) {  // a[i]
            if(tmp.index()==0) {
                std::cerr<<"const variable used as array"<<std::endl;
                return;
            }
            std::string alloc=std::get<std::string>(tmp);
            std::string prev=alloc;
            PTR p=type_check[ident];
            auto it=indexes->begin();
            if(p.pt==PTR_PTR) {
                std::cout<<"    %"<<tmp_cnt++<<" = load "<<alloc<<std::endl;
                p=*(p.base);
                // p只能是ptr_arr, ptr_int
                prev="%"+std::to_string(tmp_cnt-1);
                if(p.pt==PTR_INT) {
                    if (indexes->size() != 1) {
                        std::cerr << "ptr_int access wrong" << std::endl;
                        return;
                    }
                    var tmp2;
                    (*it)->Dump(tmp2);
                    int operand = tmp_cnt - 1;
                    if (tmp2.index() == 0) {
                        std::cout << "    %" << tmp_cnt++ << " = getptr " << prev << ", " << std::get<int>(tmp2)
                                  << std::endl;
                    } else if (tmp2.index() == 1) {
                        std::cout << "    %" << tmp_cnt++ << " = getptr " << prev << ", %" << operand << std::endl;
                    }
                    res = var(tmp_cnt - 1);  // 指针所在的局部变量
                    return;
                }else if(p.pt==PTR_ARR) {
                    var tmp2;
                    (*it)->Dump(tmp2);
                    int operand = tmp_cnt - 1;
                    it++;
                    if (tmp2.index() == 0) {
                        std::cout << "    %" << tmp_cnt++ << " = getptr " << prev << ", " << std::get<int>(tmp2)
                                  << std::endl;
                    } else if (tmp2.index() == 1) {
                        std::cout << "    %" << tmp_cnt++ << " = getptr " << prev << ", %" << operand << std::endl;
                    }
                    prev="%"+std::to_string(tmp_cnt-1);
                    // 接下去处理
                }
            }
            for(;it!=indexes->end();it++) {
                var tmp;
                (*it)->Dump(tmp);
                int operand=tmp_cnt-1;
                if(tmp.index()==0) {
                    std::cout<<"    %"<<tmp_cnt++<<" = getelemptr "<<prev<<", "<<std::get<int>(tmp)<<std::endl;
                }else if(tmp.index()==1) {
//                    std::cerr<<"may have problem"<<std::endl;  // no problem a[i]
                    std::cout<<"    %"<<tmp_cnt++<<" = getelemptr "<<prev<<", "<<'%'<<operand<<std::endl;
                }
                prev="%"+std::to_string(tmp_cnt-1);
                p=*(p.base);
            }
            if(res.index()==1) {
                std::string mode=std::get<std::string>(res);
                if(mode.compare("param")==0) {
                    if(p.pt==PTR_ARR) {  // 传递数组参数相当于传递其第一个元素的地址
                        std::cout<<"    %"<<tmp_cnt++<<" = getelemptr "<<prev<<", 0"<<std::endl;
                        return;
                    }
                }
            }
            res=var(tmp_cnt-1);  // 指针所在的局部变量
        }else {
            if(res.index()==1) {
                PTR p=type_check[ident];
                std::string mode=std::get<std::string>(res);
                if(mode.compare("param")) {  // 不是处理参数
                    std::cerr<<"function param have problem"<<std::endl;
                    return;
                }
                if(p.pt==PTR_INT||p.pt==PTR_PTR) {
                    res=var(ident);
                }
                else if(p.pt==PTR_ARR) {
                    std::string alloc=std::get<std::string>(tmp);
                    std::cout<<"    %"<<tmp_cnt++<<" = getelemptr "<<alloc<<", 0"<<std::endl;
                }
            }else {
                res=var(ident);  // 不做处理，传给primaryExp处理
            }
        }
    }
    void Dump(bool &hasret)  override{}
};

class Stmt_withoutIFAST:public BaseAST {
public:
    // std:string ret="return";
    bool ret=false;
    std::unique_ptr<BaseAST> exp;
    std::unique_ptr<BaseAST> lval;
    // "="
    // exp
    std::unique_ptr<BaseAST> block;

    std::unique_ptr<BaseAST> while_stmt;

    std::unique_ptr<BaseAST> Break;
    std::unique_ptr<BaseAST> Continue;

    void Dump(bool &hasret)  override {
        var tmp;
        if(block) {
            symTable* saved=top;
            top=new symTable(top);
            block->Dump(hasret);
            delete top;
            top=saved;
            return;
        }
        if(while_stmt) {
            while_stmt->Dump();
            return;
        }
        if(Break) {
            Break->Dump(hasret);
            return;
        }
        if(Continue) {
            Continue->Dump(hasret);
            return;
        }
        if(!lval&&!block&&!ret) {  // [Exp] ;
            if (exp) {
                var tmp2;
                exp->Dump(tmp2);
                return;
            }
            else return;
        }
        if(!exp) {  // return;
            std::cout<<"    ret"<<std::endl;
            hasret=true;
            return;
        }
        exp->Dump(tmp);
        int operand=BaseAST::tmp_cnt-1;
        if(tmp.index()==0&&ret) {  // return 0;
            std::cout << "    ret " <<std::get<int>(tmp) << std::endl;
            hasret = true;
        }else if(tmp.index()==1&&lval) {  // x= x+1; or a[i]=x+1;
            var tmp2;
            lval->Dump(tmp2);
            if(tmp2.index()==0) {  // a[i]=x+1;
                std::cout<<"    store %"<<operand<<", %"<<std::get<int>(tmp2)<<std::endl;
            }else if(tmp2.index()==1) {  // x=x+1
                std::string variable=std::get<std::string>(tmp2);
                std::string alloc=std::get<std::string>(top->getValue(variable));
                std::cout<<"    store %"<<operand<<", "<<alloc<<std::endl;
            }
        }else if(tmp.index()==0&&lval) {  // x= 1; or a[i]=1;
            var tmp2;
            lval->Dump(tmp2);
            if(tmp2.index()==0) {  // a[i]=1
                std::cout<<"    store "<<std::get<int>(tmp)<<", %"<<std::get<int>(tmp2)<<std::endl;
            }else if(tmp2.index()==1) { // x=1
                std::string variable=std::get<std::string>(tmp2);
                std::string alloc=std::get<std::string>(top->getValue(variable));
                std::cout<<"    store "<<std::get<int>(tmp)<<", "<<alloc<<std::endl;
            }
        }else if(tmp.index()==1&&ret) {  // return x;
            std::cout<<"    ret %"<<BaseAST::tmp_cnt-1<<std::endl;
            hasret=true;
        }
    }
    void Dump(var &res) override {}
    void Dump() override {}
};

class BreakAST:public BaseAST {
public:
    void Dump(bool &hasret) override{
        std::string BFalse=std::get<std::string>(top->getValue(std::string("break")));
        std::cout<<"    jump "<<BFalse<<std::endl;
        hasret=true;
    }
    void Dump(var &res) override {}
    void Dump() override {}
};

class ContinueAST:public BaseAST {
public:
    void Dump(bool &hasret) override{
        std::string entry=std::get<std::string>(top->getValue(std::string("continue")));
        std::cout<<"    jump "<<entry<<std::endl;
        hasret=true;  // 这里hasret表明块中已经有跳转了
    }
    void Dump(var &res) override {}
    void Dump() override {}
};

class Matched_StmtAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> other;
    std::unique_ptr<BaseAST> exp;
    std::unique_ptr<BaseAST> then_stmt;
    std::unique_ptr<BaseAST> else_stmt;
    void Dump() override {
        if(other) other->Dump();
    }
    void Dump(var &res) override {
        if(other) other->Dump(res);
    }
    void Dump(bool &hasret) override {
        if(other) {
            other->Dump(hasret);
            return;
        }

        if_cnt++;
        std::string next="%"+func_name+"_end";
        next+=std::to_string(if_cnt);
        std::string BTrue="%"+func_name+"_then";
        BTrue+=std::to_string(if_cnt);
        std::string BFalse="%"+func_name+"_else";
        BFalse+=std::to_string(if_cnt);

        var tmp;
        exp->Dump(tmp);
        if(tmp.index()==1) {  // exp是表达式
            int operand=BaseAST::tmp_cnt-1;
            std::cout<<"    br %"<<operand<<", "<<BTrue<<", "<<BFalse<<std::endl;
        }else if(tmp.index()==0) {  // exp是常数值
            std::cout<<"    br "<<std::get<int>(tmp)<<", "<<BTrue<<", "<<BFalse<<std::endl;
        }
        std::cout<<std::endl;
        std::cout<<BTrue<<":"<<std::endl;
        bool then_ret=false;
        then_stmt->Dump(then_ret);
        if(!then_ret)  // then语句块中没有if才jump
            std::cout<<"    jump "<<next<<std::endl;
        bool else_ret=false;
        std::cout<<std::endl;
        std::cout<<BFalse<<":"<<std::endl;
        else_stmt->Dump(else_ret);
        if(!else_ret) {  // else语句块中没有ret才有jump
            std::cout << "    jump " << next << std::endl;
        }
        if(then_ret&&else_ret) {  // then else 都有ret，之后的代码不再处理
            hasret=true;
        }
        if(!hasret) {
            std::cout << std::endl;
            std::cout << next << ":" << std::endl;
        }
    }
};

class Open_StmtAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> exp;
    std::unique_ptr<BaseAST> then_stmt;
    std::unique_ptr<BaseAST> else_stmt;
    void Dump() override {

    }
    void Dump(var &res)  override {

    }
    void Dump(bool &hasret) override {
        if_cnt++;
        std::string next="%"+func_name+"_end";
        next+=std::to_string(if_cnt);
        std::string BTrue="%"+func_name+"_then";
        BTrue+=std::to_string(if_cnt);
        std::string BFalse;
        if(else_stmt) {
            BFalse="%"+func_name+"_else";
            BFalse+=std::to_string(if_cnt);
        }else {
            BFalse=next;
        }
        var tmp;
        exp->Dump(tmp);
        if(tmp.index()==1) {  // exp是表达式
            int operand=BaseAST::tmp_cnt-1;
            std::cout<<"    br %"<<operand<<", "<<BTrue<<", "<<BFalse<<std::endl;
        }else if(tmp.index()==0) {  // exp是常数值
            std::cout<<"    br "<<std::get<int>(tmp)<<", "<<BTrue<<", "<<BFalse<<std::endl;
        }
        std::cout<<std::endl;
        std::cout<<BTrue<<":"<<std::endl;
        bool then_ret=false;
        then_stmt->Dump(then_ret);
        if(!then_ret)  // then语句块中没有if才jump
            std::cout<<"    jump "<<next<<std::endl;
        bool else_ret=false;
        if(else_stmt) {
            std::cout<<std::endl;
            std::cout<<BFalse<<":"<<std::endl;
            else_stmt->Dump(else_ret);
            if(!else_ret) {  // else语句块中没有ret才有jump
                std::cout << "    jump " << next << std::endl;
            }
        }
        if(then_ret&&else_ret) {  // then else 都有ret，之后的代码不再处理
            hasret=true;
        }
        if(!hasret) {
            std::cout << std::endl;
            std::cout << next << ":" << std::endl;
        }
    }
};

class StmtAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> open;
    std::unique_ptr<BaseAST> matched;
    void Dump() override {
        if(open) open->Dump();
        else if(matched) matched->Dump();
    }
    void Dump(var &res) override {
        if(open) open->Dump(res);
        else if(matched) matched->Dump(res);
    }
    void Dump(bool &hasret) override {
        if(open) open->Dump(hasret);
        else if(matched) matched->Dump(hasret);
    }
};

class ExpAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> lorexp;
    void Dump() override {
        lorexp->Dump();
    }
    void Dump(var &res) override {
        lorexp->Dump(res);
    }
    void Dump(bool &hasret)  override {}
};

class PrimaryExpAST:public BaseAST {
public:
    //std::string left="(";
    std::unique_ptr<BaseAST> exp;
    //std::string right=")";

    std::unique_ptr<BaseAST> lval;

    int number;
    void Dump() override{}

    void Dump(var &res)  override {
        if(exp) { // (exp)
            exp->Dump(res);
        }else if(!lval) { // number
            res=number;
        }else {
            var tmp=res;
            lval->Dump(tmp);
            if(tmp.index()==0) {  // a[i]
                std::cout<<"    %"<<tmp_cnt++<<" = load %"<<std::get<int>(tmp)<<std::endl;
                res=var("variable");
            }else if(tmp.index()==1) {  // x
                std::string mode=std::get<std::string>(tmp);
                if(mode.compare("param")==0) {
                    return;
                }
                var tmp2=top->getValue(std::get<std::string>(tmp)); //重新从符号表中取出来
                if(tmp2.index()==0) {
                    res=tmp2;
                }else {
                    std::string s=std::get<std::string>(tmp2);
                    std::cout << "    %" << BaseAST::tmp_cnt++ << " = load " << s << std::endl;
                    res=var("variable");
                }
            }
        }
    }
    void Dump(bool &hasret) override {

    }
};

class UnaryExpAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> primaryexp;

    std::string unaryop;
    std::unique_ptr<BaseAST> unaryexp;

    std::string ident;
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> params;  // <exp>

    void Dump() override{}

    void Dump(var &res) override {
        if(primaryexp) {
           primaryexp->Dump(res);
        }else if(unaryexp){
            var tmp;
            unaryexp->Dump(tmp);
            int operand=BaseAST::tmp_cnt-1;
            if(tmp.index()==0) {  // int
                if(!unaryop.compare(std::string("+"))) {
                    res=tmp;
                }
                else if(!unaryop.compare(std::string("-"))) {
                    res=0-std::get<int>(tmp);
                }else if(!unaryop.compare(std::string("!"))) {
                    res=!std::get<int>(tmp);
                }
            }else if(tmp.index()==1) { //string
                if(!unaryop.compare(std::string("+"))) {}
                else if(!unaryop.compare(std::string("-"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = sub 0, %"<<operand<<std::endl;
                }else if(!unaryop.compare(std::string("!"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = eq 0, %"<<operand<<std::endl;
                }
                res=std::string("variable");  // 只需要传递”这是个变量“的信息就行了，下同
            }
        }else if(ident.length()) {  // function call
            std::string funcname=ident+"()";
            if(!top->contain(funcname)) {
                std::cerr<<"Call undefined function "<<funcname<<std::endl;
                return;
            }
            std::vector<var> vec;  // params vec
            for(auto it=params->begin();it!=params->end();it++) {
                var tmp(std::string("param"));
                (*it)->Dump(tmp);
                if(tmp.index()==0) { // const number
                    vec.push_back(var(std::get<int>(tmp)));
                }else if(tmp.index()==1) { // variable
                    int param = BaseAST::tmp_cnt - 1;
                    std::string s = "%" + std::to_string(param);
                    vec.push_back(var(s));
                }
            }
            std::string func_type=std::get<std::string>(top->getValue(funcname));
            if(func_type.compare("int")==0) {
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = "<<"call @"<<ident<<"(";
            }
            else std::cout<<"    call @"<<ident<<"(";
            auto it=vec.begin();
            while(it!=vec.end()) {
                var cur=(*it);
                if(cur.index()==0) {  // const number
                    std::cout<<std::get<int>(cur);
                }else if(cur.index()==1) { // variable
                    std::cout<<std::get<std::string>(cur);
                }
                it++;
                if(it!=vec.end()) {
                    std::cout<<", ";
                }
            }
            std::cout<<")"<<std::endl;
            res=std::string("variable");
        }
    }
    void Dump(bool &hasret) override {}
};

class MulExpAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> unaryexp;

    std::unique_ptr<BaseAST> mulexp;
    std::string op;
    //std::unique_ptr<BaseAST*> unaryexp;
    void Dump() override{
        if(mulexp==NULL) {
            unaryexp->Dump();
        }else {
            mulexp->Dump();
            int operand1=BaseAST::tmp_cnt-1;  // 记录第一个操作数对应的临时变量编号，下同
            unaryexp->Dump();
            int operand2=BaseAST::tmp_cnt-1;  // 记录第二个操作数对应的临时变量编号，下同
            // 后序周游
            if(!op.compare(std::string("*"))) {
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = mul %"<<operand1<<", %"<<operand2<<std::endl;
            }else if(!op.compare(std::string("/"))) {
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = div %"<<operand1<<", %"<<operand2<<std::endl;
            }else if(!op.compare(std::string("%"))) {
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = mod %"<<operand1<<", %"<<operand2<<std::endl;
            }

        }
    }
    void Dump(var &res) override {
        if(mulexp==NULL) {
            unaryexp->Dump(res);
        }else {
            var tmp1,tmp2;
            mulexp->Dump(tmp1);
            int operand1=BaseAST::tmp_cnt-1;
            unaryexp->Dump(tmp2);
            int operand2=BaseAST::tmp_cnt-1;
            if(tmp1.index()==0&&tmp2.index()==0) {
                if(!op.compare(std::string("*"))) {
                    res=std::get<int>(tmp1)*std::get<int>(tmp2);
                }else if(!op.compare(std::string("/"))) {
                    res=std::get<int>(tmp1)/std::get<int>(tmp2);
                }else if(!op.compare(std::string("%"))) {
                    res=std::get<int>(tmp1)%std::get<int>(tmp2);
                }
            }else if(tmp1.index()==1&&tmp2.index()==0) {
                if(!op.compare(std::string("*"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = mul %"<<operand1<<", "<<std::get<int>(tmp2)<<std::endl;
                }else if(!op.compare(std::string("/"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = div %"<<operand1<<", "<<std::get<int>(tmp2)<<std::endl;
                }else if(!op.compare(std::string("%"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = mod %"<<operand1<<", "<<std::get<int>(tmp2)<<std::endl;
                }
                res=std::string("variable");  // 告诉caller表达式包含变量，需要输出
            }else if(tmp1.index()==0&&tmp2.index()==1) {
                if(!op.compare(std::string("*"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = mul "<<std::get<int>(tmp1)<<", %"<<operand2<<std::endl;
                }else if(!op.compare(std::string("/"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = div "<<std::get<int>(tmp1)<<", %"<<operand2<<std::endl;
                }else if(!op.compare(std::string("%"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = mod "<<std::get<int>(tmp1)<<", %"<<operand2<<std::endl;
                }
                res=std::string("variable");
            }else if(tmp1.index()==1&&tmp2.index()==1) {
                if(!op.compare(std::string("*"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = mul %"<<operand1<<", %"<<operand2<<std::endl;
                }else if(!op.compare(std::string("/"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = div %"<<operand1<<", %"<<operand2<<std::endl;
                }else if(!op.compare(std::string("%"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = mod %"<<operand1<<", %"<<operand2<<std::endl;
                }
                res=std::string("variable");
            }
        }
    }
    void Dump(bool &hasret) override {}
};

class AddExpAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> mulexp;

    std::unique_ptr<BaseAST> addexp;
    std::string op;
    //std::unique_ptr<BaseAST> mulexp;
    void Dump() override{
        if(addexp==NULL) {
            mulexp->Dump();
        }else {
            addexp->Dump();
            int operand1=BaseAST::tmp_cnt-1;
            mulexp->Dump();  // 后序周游
            int operand2=BaseAST::tmp_cnt-1;
            if(!op.compare(std::string("+"))) {
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = add %"<<operand1<<", %"<<operand2<<std::endl;
            }else if(!op.compare(std::string("-"))) {
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = sub %"<<operand1<<", %"<<operand2<<std::endl;
            }
        }
    }
    void Dump(var &res) override {
        if(addexp==NULL) {
            mulexp->Dump(res);
        }else {
            var tmp1,tmp2;
            addexp->Dump(tmp1);
            int operand1=BaseAST::tmp_cnt-1;
            mulexp->Dump(tmp2);
            int operand2=BaseAST::tmp_cnt-1;
            if(tmp1.index()==0&&tmp2.index()==0) {
                if(!op.compare(std::string("+"))) {
                    res=std::get<int>(tmp1)+std::get<int>(tmp2);
                }else if(!op.compare(std::string("-"))) {
                    res=std::get<int>(tmp1)-std::get<int>(tmp2);
                }
            }else if(tmp1.index()==1&&tmp2.index()==0) {
                if(!op.compare(std::string("+"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = add %"<<operand1<<", "<<std::get<int>(tmp2)<<std::endl;
                }else if(!op.compare(std::string("-"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = sub %"<<operand1<<", "<<std::get<int>(tmp2)<<std::endl;
                }
                res=std::string("variable");  // 告诉caller表达式包含变量，需要输出
            }else if(tmp1.index()==0&&tmp2.index()==1) {
                if(!op.compare(std::string("+"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = add "<<std::get<int>(tmp1)<<", %"<<operand2<<std::endl;
                }else if(!op.compare(std::string("-"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = sub "<<std::get<int>(tmp1)<<", %"<<operand2<<std::endl;
                }
                res=std::string("variable");
            }else if(tmp1.index()==1&&tmp2.index()==1) {
                if(!op.compare(std::string("+"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = add %"<<operand1<<", %"<<operand2<<std::endl;
                }else if(!op.compare(std::string("-"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = sub %"<<operand1<<", %"<<operand2<<std::endl;
                }
                res=std::string("variable");
            }
        }
    }
    void Dump(bool &hasret) override {}
};

class RelExpAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> addexp;

    std::unique_ptr<BaseAST> relexp;
    std::string op;
    //std::unique_ptr<BaseAST> addexp;
    void Dump() override {
        if(relexp==NULL) {
            addexp->Dump();
        }else {
            relexp->Dump();
            int operand1=BaseAST::tmp_cnt-1;
            addexp->Dump();
            int operand2=BaseAST::tmp_cnt-1;
            if(!op.compare(std::string("<"))) {
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = lt %"<<operand1<<", %"<<operand2<<std::endl;
            }else if(!op.compare(std::string(">"))) {
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = gt %"<<operand1<<", %"<<operand2<<std::endl;
            }else if(!op.compare(std::string("<="))) {
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = le %"<<operand1<<", %"<<operand2<<std::endl;
            }else if(!op.compare(std::string(">="))) {
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = ge %"<<operand1<<", %"<<operand2<<std::endl;
            }
        }
    }
    void Dump(var &res) override {
        if(relexp==NULL) {
            addexp->Dump(res);
        }else {
            var tmp1,tmp2;
            relexp->Dump(tmp1);
            int operand1=BaseAST::tmp_cnt-1;
            addexp->Dump(tmp2);
            int operand2=BaseAST::tmp_cnt-1;
            if(tmp1.index()==0&&tmp2.index()==0) {
                if(!op.compare(std::string("<"))) {
                    res=std::get<int>(tmp1)<std::get<int>(tmp2);
                }else if(!op.compare(std::string(">"))) {
                    res=std::get<int>(tmp1)>std::get<int>(tmp2);
                }else if(!op.compare(std::string("<="))) {
                    res=std::get<int>(tmp1)<=std::get<int>(tmp2);
                }else if(!op.compare(std::string(">="))) {
                    res=std::get<int>(tmp1)>=std::get<int>(tmp2);
                }
            }else if(tmp1.index()==1&&tmp2.index()==0) {
                if(!op.compare(std::string("<"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = lt %"<<operand1<<", "<<std::get<int>(tmp2)<<std::endl;
                }else if(!op.compare(std::string(">"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = gt %"<<operand1<<", "<<std::get<int>(tmp2)<<std::endl;
                }else if(!op.compare(std::string("<="))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = le %"<<operand1<<", "<<std::get<int>(tmp2)<<std::endl;
                }else if(!op.compare(std::string(">="))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = ge %"<<operand1<<", "<<std::get<int>(tmp2)<<std::endl;
                }
                res=std::string("variable");  // 告诉caller表达式包含变量，需要输出
            }else if(tmp1.index()==0&&tmp2.index()==1) {
                if(!op.compare(std::string("<"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = lt "<<std::get<int>(tmp1)<<", %"<<operand2<<std::endl;
                }else if(!op.compare(std::string(">"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = gt "<<std::get<int>(tmp1)<<", %"<<operand2<<std::endl;
                }else if(!op.compare(std::string("<="))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = le "<<std::get<int>(tmp1)<<", %"<<operand2<<std::endl;
                }else if(!op.compare(std::string(">="))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = ge "<<std::get<int>(tmp1)<<", %"<<operand2<<std::endl;
                }
                res=std::string("variable");
            }else if(tmp1.index()==1&&tmp2.index()==1) {
                if(!op.compare(std::string("<"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = lt %"<<operand1<<", %"<<operand2<<std::endl;
                }else if(!op.compare(std::string(">"))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = ge %"<<operand1<<", %"<<operand2<<std::endl;
                }else if(!op.compare(std::string("<="))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = le %"<<operand1<<", %"<<operand2<<std::endl;
                }else if(!op.compare(std::string(">="))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = ge %"<<operand1<<", %"<<operand2<<std::endl;
                }
                res=std::string("variable");
            }

        }
    }
    void Dump(bool &hasret) override {}
};

class EqExpAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> relexp;

    std::unique_ptr<BaseAST> eqexp;
    std::string op;
    //std::unique_ptr<BaseAST> relexp;
    void Dump() override {
        if(eqexp==NULL) {
            relexp->Dump();
        }else {
            eqexp->Dump();
            int operand1=BaseAST::tmp_cnt-1;
            relexp->Dump();
            int operand2=BaseAST::tmp_cnt-1;
            if(!op.compare(std::string("=="))) {
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = eq %"<<operand1<<", %"<<operand2<<std::endl;
            }else if(!op.compare(std::string("!="))) {
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = ne %"<<operand1<<", %"<<operand2<<std::endl;
            }
        }
    }
    void Dump(var &res) override {
        if(eqexp==NULL) {
            relexp->Dump(res);
        }else {
            var tmp1,tmp2;
            eqexp->Dump(tmp1);
            int operand1=BaseAST::tmp_cnt-1;
            relexp->Dump(tmp2);
            int operand2=BaseAST::tmp_cnt-1;
            if(tmp1.index()==0&&tmp2.index()==0) {
                if(!op.compare(std::string("=="))) {
                    res=std::get<int>(tmp1)==std::get<int>(tmp2);
                }else if(!op.compare(std::string("!="))) {
                    res=std::get<int>(tmp1)!=std::get<int>(tmp2);
                }
            }else if(tmp1.index()==1&&tmp2.index()==0) {
                if(!op.compare(std::string("=="))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = eq %"<<operand1<<", "<<std::get<int>(tmp2)<<std::endl;
                }else if(!op.compare(std::string("!="))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = ne %"<<operand1<<", "<<std::get<int>(tmp2)<<std::endl;
                }
                res=std::string("variable");  // 告诉caller表达式包含变量，需要输出
            }else if(tmp1.index()==0&&tmp2.index()==1) {
                if(!op.compare(std::string("=="))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = eq "<<std::get<int>(tmp1)<<", %"<<operand2<<std::endl;
                }else if(!op.compare(std::string("!="))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = ne "<<std::get<int>(tmp1)<<", %"<<operand2<<std::endl;
                }
                res=std::string("variable");
            }else if(tmp1.index()==1&&tmp2.index()==1) {
                if(!op.compare(std::string("=="))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = eq %"<<operand1<<", %"<<operand2<<std::endl;
                }else if(!op.compare(std::string("!="))) {
                    std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = ne %"<<operand1<<", %"<<operand2<<std::endl;
                }
                res=std::string("variable");
            }

        }
    }
    void Dump(bool &hasret) override {}
};

class LAndExpAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> eqexp;

    std::unique_ptr<BaseAST> landexp;
    std::string op;
    //std::unique_ptr<BaseAST> eqexp;
    void Dump() override {
        if(landexp==NULL) {
            eqexp->Dump();
        }else {

            landexp->Dump();
            int operand1=BaseAST::tmp_cnt-1;


            eqexp->Dump();
            int operand2=BaseAST::tmp_cnt-1;
            if(!op.compare(std::string("&&"))) {
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = ne 0, %"<<operand1<<std::endl;
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = ne 0, %"<<operand2<<std::endl;
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = and %"<<BaseAST::tmp_cnt-3<<", %"<<BaseAST::tmp_cnt-2<<std::endl;
            }

        }
    }

    void Dump(var &res) override;
    void Dump(bool &hasret) override {}
};

class LOrExpAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> landexp;

    std::unique_ptr<BaseAST> lorexp;
    std::string op;
    //std::unique_ptr<BaseAST> landexp;
    void Dump() override {
        if(lorexp==NULL) {
            landexp->Dump();
        }else {
            lorexp->Dump();
            int operand1=BaseAST::tmp_cnt-1;
            landexp->Dump();
            int operand2=BaseAST::tmp_cnt-1;
            if(!op.compare(std::string("||"))) {
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = or %"<<operand1<<", %"<<operand2<<std::endl;
                std::cout<<"    "<<"%"<<BaseAST::tmp_cnt++<<" = ne 0, %"<<BaseAST::tmp_cnt-2<<std::endl;
            }
        }

    }
    void Dump(var &res) override;
    void Dump(bool &hasret) override {}
};



class DeclAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> constdecl;

    std::unique_ptr<BaseAST> vardecl;
    void Dump(bool &hasret) override {
        if(constdecl)
            constdecl->Dump();
        else
            vardecl->Dump();
    }
    void Dump(var &res) override {}
    void Dump() override {
        if(constdecl)
            constdecl->Dump();
        else
            vardecl->Dump();
    }
};

class ConstDeclAST:public BaseAST {
public:
    std::string btype;
    std::unique_ptr<BaseAST> constdef;
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> constdef_op;
    void Dump() override {
        constdef->Dump();
        if(!constdef_op) return;
        for(auto it=constdef_op->begin();it!=constdef_op->end();it++) {
            (*it)->Dump();
        }
    }
    void Dump(var &res) override {
        this->Dump();
    }
    void Dump(bool &hasret) override {
        this->Dump();
    }
};


class ConstInitValAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> constexp;
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> arrinit;
    void Dump() override {}
    void Dump(var &res)  override {
        constexp->Dump(res);
    }
    void Dump(bool &hasret) override {}
};

class ConstExpAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> exp;
    void Dump() override {}
    void Dump(var &res) override {
        exp->Dump(res);
    }
    void Dump(bool &hasret) override {}
};


class ConstDefAST:public BaseAST {
public:
    std::string ident;
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> indexes;
    std::unique_ptr<BaseAST> constinitval;

    std::vector<int> arr;  // 存放填充后的数组元素
    std::vector<int>::iterator iter;  // 用来打印结果的辅助变量
    /**
     *
     * @param dim 维度的长度数组，dim[i]=len表示第i维的元素总个数，如a[3][2][4],三维，dim[1]=3*2*4=24;dim[2]=2*4=8;dim[3]=4;
     * @param begin 从第几维开始填充，也即遇到初始化列表对应的维度开始
     * @param dimNum 数组的长度
     * @param init 处理的对象
     * @return 填充了多少位
     */
    int padding(int* dim, int dimNum, int begin, ConstInitValAST* init) {
        // 是列表
        int cnt=0;  // 处理了多少个整数
        int b=begin;
        for(auto it=init->arrinit->begin();it!=init->arrinit->end();) {
            ConstInitValAST* cur=(ConstInitValAST*)(*it).get();
            while(it!=init->arrinit->end()&&cur->constexp) {  // 处理整数
                var tmp;
                (cur->constexp)->Dump(tmp);
                arr.push_back(std::get<int>(tmp));
                cnt+=1;
                it++;
                cur=(ConstInitValAST*)(*it).get();
            }
            // 更新处理完整数后所在的维度
            for(int i=dimNum;i>1;i--) {
                if(cnt%dim[i]==0) {
                    b=i-1;
                }
            }
            // 处理列表
            if(cur) {
                int padNum= padding(dim,dimNum, b, cur);
                cnt+=padNum;
                // 列表可能不完整补充0
                for(int i=padNum;i<dim[b+1];i++) {
                    arr.push_back(0);
                    cnt+=1;
                }
                it++;
            }
        }

        return cnt;
    }

    void printArr(int* dim, int dimNum, int curDim) {
        std::cout<<'{';
        if(curDim==dimNum) {  // 第n维，直接输出数字
            int n=dim[curDim];
            for(int i=0;i<n;) {
                std::cout<<(*iter);
                iter++;
                i++;
                if(i<n) std::cout<<", ";
            }
        }else {
            int n=dim[curDim];
            for(int i=0;i<n;) {
                printArr(dim, dimNum,curDim+1);
                i++;
                if(i<n) std::cout<<", ";
            }
        }
        std::cout<<'}';
    }

    void getElemPtr(int* dim, int dimNum, std::string src, int curDim) {
        if(curDim==dimNum) {  // 最后一维
            for(int i=0;i<dim[curDim];i++) {
                std::cout<<"    %"<<tmp_cnt++<<" = getelemptr "<<src<<", "<<i<<std::endl;
                std::cout<<"    store "<<(*iter)<<", %"<<tmp_cnt-1<<std::endl;
                iter++;
            }
            return;
        }else {  // 不是最后一维，递归
            for(int i=0;i<dim[curDim];i++) {
                std::cout<<"    %"<<tmp_cnt++<<" = getelemptr "<<src<<", "<<i<<std::endl;
                std::string arg="%"+std::to_string(tmp_cnt-1);
                getElemPtr(dim,dimNum,arg,curDim+1);
            }
            return;
        }
    }

    void Dump() override {
        if(top==global) {
            std::string alloc_ident="@"+ident;
            if(indexes->size()!=0) { // 全局数组
                int dimNum=indexes->size();
                int* dim=new int[dimNum+1];
                int i=1;  // 暂时用来计数
                for(auto it=indexes->begin();it!=indexes->end();it++) {
                    var tmp;
                    (*it)->Dump(tmp);
                    if(tmp.index()!=0) {
                        std::cerr<<"arr index is not const numbers."<<std::endl;
                        return;
                    }
                    dim[i++]=std::get<int>(tmp);
                }
                //如a[3][2][4],三维，dim2[1]=3*2*4=24;dim2[2]=2*4=8;dim2[3]=4;
                int* dim2=new int[dimNum+1];
                dim2[dimNum]=dim[dimNum];
                for(int i=dimNum;i>1;i--) {
                    dim2[i-1]=dim2[i]*dim[i-1];
                }
                std::cout<<"global "<<alloc_ident<<" = alloc ";
                for(int i=1;i<dimNum;i++) std::cout<<'[';
                std::cout<<"[ i32, "<<dim[dimNum]<<']';
                for(int i=dimNum-1;i>=1;i--) {
                    std::cout<<", "<<dim[i]<<']';
                }
                std::cout<<", ";
                if(!constinitval) {
                    std::cout<<"zeroinit"<<std::endl;
                }else {
                    // initval一定是常量表达式
                    ConstInitValAST* ini=(ConstInitValAST*)constinitval.get();
                    int padNum=padding(dim2, dimNum, 1, ini);
                    // 最后再补满
                    while(padNum<dim2[1]) {
                        arr.push_back(0);
                        padNum++;
                    }
                    // 输出
                    iter=arr.begin();
                    printArr(dim, dimNum, 1);
                    std::cout<<std::endl;
                }
                top->insert(ident,var(alloc_ident));
                PTR* first=new PTR();
                PTR* p=first;
                for(int i=1;i<=dimNum;i++) {
                    p->pt=PTR_ARR;
                    p->base=new PTR();
                    p=p->base;
                }
                p->pt=PTR_INT;
                type_check[ident]=*first;
                return;
            }else {  // 全局常量
                var tmp;
                constinitval->Dump(tmp);
                if(tmp.index()==0)  // int， 直接插入符号表
                    top->insert(ident, tmp);
                return;
            }
            return;
        }
        std::string alloc_ident="@"+ident;
        if(top->contain(ident)) alloc_ident=std::get<std::string>(top->getValue(ident))+"_"+std::to_string(tmp_cnt);  // 如果外层定义过，追加_+tmp_cnt
        // 局部数组
        if(indexes&&indexes->size()) {
            int dimNum=indexes->size();
            int* dim=new int[dimNum+1];
            int i=1;  // 暂时用来计数
            for(auto it=indexes->begin();it!=indexes->end();it++) {
                var tmp;
                (*it)->Dump(tmp);
                if(tmp.index()!=0) {
                    std::cerr<<"arr index is not const numbers."<<std::endl;
                    return;
                }
                dim[i++]=std::get<int>(tmp);
            }
            //如a[3][2][4],三维，dim2[1]=3*2*4=24;dim2[2]=2*4=8;dim2[3]=4;
            int* dim2=new int[dimNum+1];
            dim2[dimNum]=dim[dimNum];
            for(int i=dimNum;i>1;i--) {
                dim2[i-1]=dim2[i]*dim[i-1];
            }
            std::cout<<"    "<<alloc_ident<<" = alloc ";
            for(int i=1;i<dimNum;i++) std::cout<<'[';
            std::cout<<"[ i32, "<<dim[dimNum]<<']';
            for(int i=dimNum-1;i>=1;i--) {
                std::cout<<", "<<dim[i]<<']';
            }
            std::cout<<std::endl;
            if(!constinitval) {
                for(int i=0;i<dim2[1];i++) {  // 填满0
                    arr.push_back(0);
                }
            }else {
                // initval一定是常量表达式
                ConstInitValAST* ini=(ConstInitValAST*)constinitval.get();
                int padNum=padding(dim2, dimNum, 1, ini);
                // 最后再补满
                while(padNum<dim2[1]) {
                    arr.push_back(0);
                    padNum++;
                }
            }
            // 输出
            iter=arr.begin();
            getElemPtr(dim,dimNum,alloc_ident,1);
            top->insert(ident,var(alloc_ident));
            PTR* first=new PTR();
            PTR* p=first;
            for(int i=1;i<=dimNum;i++) {
                p->pt=PTR_ARR;
                p->base=new PTR();
                p=p->base;
            }
            p->pt=PTR_INT;
            type_check[ident]=*first;
            return;
        }
        // 局部常量
        var tmp;
        constinitval->Dump(tmp);
        if(tmp.index()==0)  // int， 直接插入符号表
            top->insert(ident, tmp);
        else {  // 临时变量，需要分配alloc
//            std::cout<<"    "<<alloc_ident<<" = alloc i32"<<std::endl;
//            std::cout<<"    "<<"store %"<<tmp_cnt-1<<", "<<alloc_ident<<std::endl;
//            var alloc=alloc_ident;
//            top->insert(ident, alloc);
              std::cerr<<"const variable can't calc its value in compileing"<<std::endl;
        }
    }
    void Dump(var &res) override {
        this->Dump();
    }
    void Dump(bool &hasret) override {
        this->Dump();
    }
};


class VarDeclAST:public BaseAST {
public:
    std::string btype;
    std::unique_ptr<BaseAST> vardef;
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> vardef_op;
    void Dump() override {
        vardef->Dump();
        if(!vardef_op) return;
        for(auto it=vardef_op->begin();it!=vardef_op->end();it++) {
            (*it)->Dump();
        }
    }
    void Dump(var &res) override {
        this->Dump();
    }
    void Dump(bool &hasret) override {
        this->Dump();  // 会通过这个函数调用Dump()
    }
};


class InitValAST:public BaseAST {
public:
    std::unique_ptr<BaseAST> exp;
    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> arrinit;  // 元素是InitVal
    void Dump() override {}
    void Dump(var &res) override {
        if(exp) exp->Dump(res);
        else if(arrinit->size()) {

        }
    }
    void Dump(bool &hasret) override {}
};

class VarDefAST:public BaseAST {
public:
    std::string ident;

    std::unique_ptr<std::vector<std::unique_ptr<BaseAST>>> indexes;


    //std::string ident;
    std::unique_ptr<BaseAST> initval;

    std::vector<int> arr;  // 存放填充后的数组元素
    std::vector<int>::iterator iter;  // 用来打印结果的辅助变量
    /**
     *
     * @param dim 维度的长度数组，dim[i]=len表示第i维的元素总个数，如a[3][2][4],三维，dim[1]=3*2*4=24;dim[2]=2*4=8;dim[3]=4;
     * @param begin 从第几维开始填充，也即遇到初始化列表对应的维度开始
     * @param dimNum 数组的长度
     * @param init 处理的对象
     * @return 填充了多少位
     */
    int padding(int* dim, int dimNum, int begin, InitValAST* init) {
        // 是列表
        int cnt=0;  // 处理了多少个整数
        int b=begin;
        for(auto it=init->arrinit->begin();it!=init->arrinit->end();) {
            InitValAST* cur=(InitValAST*)(*it).get();
            while(it!=init->arrinit->end()&&cur->exp) {  // 处理整数
                var tmp;
                (cur->exp)->Dump(tmp);
                arr.push_back(std::get<int>(tmp));
                cnt+=1;
                it++;
                cur=(InitValAST*)(*it).get();
            }
            // 更新处理完整数后所在的维度
            for(int i=dimNum;i>1;i--) {
                if(cnt%dim[i]==0) {
                    b=i-1;
                }
            }
            // 处理列表
            if(cur) {
                int padNum= padding(dim,dimNum, b, cur);
                cnt+=padNum;
                // 列表可能不完整补充0
                for(int i=padNum;i<dim[b+1];i++) {
                    arr.push_back(0);
                    cnt+=1;
                }
                it++;
            }
        }

        return cnt;
    }

    void printArr(int* dim, int dimNum, int curDim) {
        std::cout<<'{';
        if(curDim==dimNum) {  // 第n维，直接输出数字
            int n=dim[curDim];
            for(int i=0;i<n;) {
                std::cout<<(*iter);
                iter++;
                i++;
                if(i<n) std::cout<<", ";
            }
        }else {
            int n=dim[curDim];
            for(int i=0;i<n;) {
                printArr(dim, dimNum,curDim+1);
                i++;
                if(i<n) std::cout<<", ";
            }
        }
        std::cout<<'}';
    }

    void getElemPtr(int* dim, int dimNum, std::string src, int curDim) {
        if(curDim==dimNum) {  // 最后一维
            for(int i=0;i<dim[curDim];i++) {
                std::cout<<"    %"<<tmp_cnt++<<" = getelemptr "<<src<<", "<<i<<std::endl;
                std::cout<<"    store "<<(*iter)<<", %"<<tmp_cnt-1<<std::endl;
                iter++;
            }
            return;
        }else {  // 不是最后一维，递归
            for(int i=0;i<dim[curDim];i++) {
                std::cout<<"    %"<<tmp_cnt++<<" = getelemptr "<<src<<", "<<i<<std::endl;
                std::string arg="%"+std::to_string(tmp_cnt-1);
                getElemPtr(dim,dimNum,arg,curDim+1);
            }
            return;
        }
    }

    void Dump() override {
        if(top==global) {
            if(top->contain(ident)) {
                std::cerr<<"duplicated defined global variable"<<std::endl;
                return;
            }
            std::string alloc_ident="@"+ident;
            if(indexes->size()!=0) { // 全局数组
                int dimNum=indexes->size();
                int* dim=new int[dimNum+1];
                int i=1;  // 暂时用来计数
                for(auto it=indexes->begin();it!=indexes->end();it++) {
                    var tmp;
                    (*it)->Dump(tmp);
                    if(tmp.index()!=0) {
                        std::cerr<<"arr index is not const numbers."<<std::endl;
                        return;
                    }
                    dim[i++]=std::get<int>(tmp);
                }
                //如a[3][2][4],三维，dim2[1]=3*2*4=24;dim2[2]=2*4=8;dim2[3]=4;
                int* dim2=new int[dimNum+1];
                dim2[dimNum]=dim[dimNum];
                for(int i=dimNum;i>1;i--) {
                    dim2[i-1]=dim2[i]*dim[i-1];
                }
                std::cout<<"global "<<alloc_ident<<" = alloc ";
                for(int i=1;i<dimNum;i++) std::cout<<'[';
                std::cout<<"[ i32, "<<dim[dimNum]<<']';
                for(int i=dimNum-1;i>=1;i--) {
                    std::cout<<", "<<dim[i]<<']';
                }
                std::cout<<", ";
                if(!initval) {
                    std::cout<<"zeroinit"<<std::endl;
                }else {
                    // initval一定是常量表达式
                    InitValAST* ini=(InitValAST*)initval.get();
                    int padNum=padding(dim2, dimNum, 1, ini);
                    // 最后再补满
                    while(padNum<dim2[1]) {
                        arr.push_back(0);
                        padNum++;
                    }
                    // 输出
                    iter=arr.begin();
                    printArr(dim, dimNum, 1);
                    std::cout<<std::endl;
                }
                top->insert(ident,alloc_ident);
                PTR* first=new PTR();
                PTR* p=first;
                for(int i=1;i<=dimNum;i++) {
                    p->pt=PTR_ARR;
                    p->base=new PTR();
                    p=p->base;
                }
                p->pt=PTR_INT;
                type_check[ident]=*first;
                return;
            }
            // 全局变量
            std::cout<<"global "<<alloc_ident<<" = alloc i32, ";
            if(!initval) {
                std::cout<<"zeroinit"<<std::endl;
            }else {
                var tmp;
                initval->Dump(tmp);
                int operand = BaseAST::tmp_cnt - 1;
                if(tmp.index()==0) // int
                    std::cout<<std::get<int>(tmp)<<std::endl;
                else if(tmp.index()==1) {
                    std::cerr<<"may have trouble"<<std::endl;
                    std::cout<<"zeroinit"<<std::endl;
                    std::cout<<"store %"<<operand<<", "<<alloc_ident<<std::endl;
                }
            }
            top->insert(ident,var(alloc_ident));
            return;
        }

        std::string alloc_ident="@"+ident;
        if(top->contain(ident)) alloc_ident=std::get<std::string>(top->getValue(ident))+"_"+std::to_string(tmp_cnt);  // 如果外层定义过，追加_+tmp_cnt
        // 局部数组
        if(indexes&&indexes->size()) {
            int dimNum=indexes->size();
            int* dim=new int[dimNum+1];
            int i=1;  // 暂时用来计数
            for(auto it=indexes->begin();it!=indexes->end();it++) {
                var tmp;
                (*it)->Dump(tmp);
                if(tmp.index()!=0) {
                    std::cerr<<"arr index is not const numbers."<<std::endl;
                    return;
                }
                dim[i++]=std::get<int>(tmp);
            }
            //如a[3][2][4],三维，dim2[1]=3*2*4=24;dim2[2]=2*4=8;dim2[3]=4;
            int* dim2=new int[dimNum+1];
            dim2[dimNum]=dim[dimNum];
            for(int i=dimNum;i>1;i--) {
                dim2[i-1]=dim2[i]*dim[i-1];
            }
            std::cout<<"    "<<alloc_ident<<" = alloc ";
            for(int i=1;i<dimNum;i++) std::cout<<'[';
            std::cout<<"[ i32, "<<dim[dimNum]<<']';
            for(int i=dimNum-1;i>=1;i--) {
                std::cout<<", "<<dim[i]<<']';
            }
            std::cout<<std::endl;
            if(!initval) {
                for(int i=0;i<dim2[1];i++) {  // 填满0
                    arr.push_back(0);
                }
            }else {
                // initval一定是常量表达式
                InitValAST* ini=(InitValAST*)initval.get();
                int padNum=padding(dim2, dimNum, 1, ini);
                // 最后再补满
                while(padNum<dim2[1]) {
                    arr.push_back(0);
                    padNum++;
                }
            }
            // 输出
            iter=arr.begin();
            getElemPtr(dim,dimNum,alloc_ident,1);
            top->insert(ident,var(alloc_ident));
            PTR* first=new PTR();
            PTR* p=first;
            for(int i=1;i<=dimNum;i++) {
                p->pt=PTR_ARR;
                p->base=new PTR();
                p=p->base;
            }
            p->pt=PTR_INT;
            type_check[ident]=*first;
            return;
        }
        // 局部变量
        std::cout<<"    "<<alloc_ident<<" = alloc i32"<<std::endl;
        var tmp;
        if(initval) {
            initval->Dump(tmp);
            int operand = BaseAST::tmp_cnt - 1;
            if(tmp.index()==0) // int
                std::cout<<"    "<<"store "<<std::get<int>(tmp)<<", "<<alloc_ident<<std::endl;
            else if(tmp.index()==1) {
                std::cout<<"    "<<"store %"<<operand<<", "<<alloc_ident<<std::endl;
            }
        }else {
                std::cout<<"    "<<"store 0, "<<alloc_ident<<std::endl;
        }
        top->insert(ident,var(alloc_ident));
    }
    void Dump(var &res) override {
        this->Dump();
    }
    void Dump(bool &hasret) override {
        this->Dump();
    }
};


#endif //SYSY_MAKE_TEMPLATE_AST_H
