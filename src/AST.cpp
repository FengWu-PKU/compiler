//
// Created by sakura on 2022/11/19.
//
#include "AST.h"

void LAndExpAST::Dump(var &res)  {
    if (landexp == NULL) {
        eqexp->Dump(res);
    } else {
        /**
         * landexp && eqexp
         * int result=0;
         * if(landexp) {
         *      result=eqexp;
         * }
         */
         /* int result=0; */
        PrimaryExpAST *zero = new PrimaryExpAST();
        zero->number = 0;
        UnaryExpAST *unary2 = new UnaryExpAST();
        unary2->primaryexp = std::unique_ptr<BaseAST>(zero);
        MulExpAST *mulexp2 = new MulExpAST();
        mulexp2->unaryexp = std::unique_ptr<BaseAST>(unary2);
        AddExpAST *addexp2 = new AddExpAST();
        addexp2->mulexp = std::unique_ptr<BaseAST>(mulexp2);
        RelExpAST *relexp2 = new RelExpAST();
        relexp2->addexp = std::unique_ptr<BaseAST>(addexp2);
        EqExpAST *eqexp2 = new EqExpAST();
        eqexp2->relexp = std::unique_ptr<BaseAST>(relexp2);
        LAndExpAST *landexp2 = new LAndExpAST();
        landexp2->eqexp = std::unique_ptr<BaseAST>(eqexp2);
        LOrExpAST *lorexp2 = new LOrExpAST();
        lorexp2->landexp = std::unique_ptr<BaseAST>(landexp2);
        ExpAST *exp2 = new ExpAST();
        exp2->lorexp = std::unique_ptr<BaseAST>(lorexp2);
        InitValAST *init2=new InitValAST();
        init2->exp=std::unique_ptr<BaseAST>(exp2);
        VarDefAST *vardef2=new VarDefAST();
        std::string ident="result";
        ident+=std::to_string(result_cnt++);
        vardef2->ident=ident;
        vardef2->initval=std::unique_ptr<BaseAST>(init2);
        VarDeclAST *vardecl2=new VarDeclAST();
        vardecl2->vardef=std::unique_ptr<BaseAST>(vardef2);
        vardecl2->Dump();

        LOrExpAST *lorexp1 = new LOrExpAST();
        lorexp1->landexp = std::unique_ptr<BaseAST>(landexp.get());
        ExpAST *exp = new ExpAST();
        exp->lorexp = std::unique_ptr<BaseAST>(lorexp1);

        LAndExpAST *landexp3 = new LAndExpAST();
        landexp3->eqexp = std::unique_ptr<BaseAST>(eqexp.get());
        LOrExpAST *lorexp3 = new LOrExpAST();
        lorexp3->landexp = std::unique_ptr<BaseAST>(landexp3);
        ExpAST *exp3 = new ExpAST();
        exp3->lorexp = std::unique_ptr<BaseAST>(lorexp3);
        LValAST* lval3=new LValAST();
        lval3->ident=ident;
        Stmt_withoutIFAST* other3=new Stmt_withoutIFAST();
        other3->lval=std::unique_ptr<BaseAST>(lval3);
        other3->exp=std::unique_ptr<BaseAST>(exp3);

        Open_StmtAST *os = new Open_StmtAST();
        os->exp = std::unique_ptr<BaseAST>(exp);;
        os->then_stmt = std::unique_ptr<BaseAST>(other3);
        bool dump_arg = false;
        os->Dump(dump_arg);

        // result=result!=0
        LValAST* lval4=new LValAST();
        lval4->ident=ident;
        PrimaryExpAST *result=new PrimaryExpAST();
        result->lval=std::unique_ptr<BaseAST>(lval4);
        UnaryExpAST *unary4 = new UnaryExpAST();
        unary4->primaryexp = std::unique_ptr<BaseAST>(result);
        MulExpAST *mulexp4 = new MulExpAST();
        mulexp4->unaryexp = std::unique_ptr<BaseAST>(unary4);
        AddExpAST *addexp4 = new AddExpAST();
        addexp4->mulexp = std::unique_ptr<BaseAST>(mulexp4);
        RelExpAST *relexp4 = new RelExpAST();
        relexp4->addexp = std::unique_ptr<BaseAST>(addexp4);
        EqExpAST *eqexp4 = new EqExpAST();
        eqexp4->relexp = std::unique_ptr<BaseAST>(relexp4);
        /**/
        EqExpAST *eqexp5=new EqExpAST();  // result!=0
        eqexp5->eqexp=std::unique_ptr<BaseAST>(eqexp4);  // result
        eqexp5->op=std::string("!=");
        /**/
        PrimaryExpAST *zero2 = new PrimaryExpAST();
        zero2->number = 0;
        UnaryExpAST *unary6 = new UnaryExpAST();
        unary6->primaryexp = std::unique_ptr<BaseAST>(zero2);
        MulExpAST *mulexp6 = new MulExpAST();
        mulexp6->unaryexp = std::unique_ptr<BaseAST>(unary6);
        AddExpAST *addexp6 = new AddExpAST();
        addexp6->mulexp = std::unique_ptr<BaseAST>(mulexp6);
        RelExpAST *relexp6 = new RelExpAST();
        relexp6->addexp = std::unique_ptr<BaseAST>(addexp6);
        /**/
        eqexp5->relexp=std::unique_ptr<BaseAST>(relexp6); // zero
        LAndExpAST *landexp5 = new LAndExpAST();
        landexp5->eqexp = std::unique_ptr<BaseAST>(eqexp5);
        LOrExpAST *lorexp5 = new LOrExpAST();
        lorexp5->landexp = std::unique_ptr<BaseAST>(landexp5);
        ExpAST *exp5 = new ExpAST();
        exp5->lorexp = std::unique_ptr<BaseAST>(lorexp5);
        LValAST* lval5=new LValAST();
        lval5->ident=ident;
        Stmt_withoutIFAST* other5=new Stmt_withoutIFAST();
        other5->lval=std::unique_ptr<BaseAST>(lval5);
        other5->exp=std::unique_ptr<BaseAST>(exp5);
        other5->Dump(dump_arg);

        res=std::string("variable");
    }
}


void LOrExpAST::Dump(var &res) {
    if (lorexp == NULL) {
        landexp->Dump(res);
    } else {
        /**
         * lorexp || landexp
         * int res=1;
         * if(!lorexp) {
         *      res=landexp;
         * }
         */
        /* int res=1; */
        PrimaryExpAST *one = new PrimaryExpAST();
        one->number = 1;
        UnaryExpAST *unary2 = new UnaryExpAST();
        unary2->primaryexp = std::unique_ptr<BaseAST>(one);
        MulExpAST *mulexp2 = new MulExpAST();
        mulexp2->unaryexp = std::unique_ptr<BaseAST>(unary2);
        AddExpAST *addexp2 = new AddExpAST();
        addexp2->mulexp = std::unique_ptr<BaseAST>(mulexp2);
        RelExpAST *relexp2 = new RelExpAST();
        relexp2->addexp = std::unique_ptr<BaseAST>(addexp2);
        EqExpAST *eqexp2 = new EqExpAST();
        eqexp2->relexp = std::unique_ptr<BaseAST>(relexp2);
        LAndExpAST *landexp2 = new LAndExpAST();
        landexp2->eqexp = std::unique_ptr<BaseAST>(eqexp2);
        LOrExpAST *lorexp2 = new LOrExpAST();
        lorexp2->landexp = std::unique_ptr<BaseAST>(landexp2);
        ExpAST *exp2 = new ExpAST();
        exp2->lorexp = std::unique_ptr<BaseAST>(lorexp2);
        InitValAST *init2=new InitValAST();
        init2->exp=std::unique_ptr<BaseAST>(exp2);
        VarDefAST *vardef2=new VarDefAST();
        std::string ident="result";
        ident+=std::to_string(result_cnt++);
        vardef2->ident=ident;
        vardef2->initval=std::unique_ptr<BaseAST>(init2);
        VarDeclAST *vardecl2=new VarDeclAST();
        vardecl2->vardef=std::unique_ptr<BaseAST>(vardef2);
        vardecl2->Dump();

        UnaryExpAST *not_lorexp=new UnaryExpAST();
        not_lorexp->unaryexp=std::unique_ptr<BaseAST>(lorexp.get());
        not_lorexp->unaryop="!";
        MulExpAST *mulexp1=new MulExpAST();
        mulexp1->unaryexp=std::unique_ptr<BaseAST>(not_lorexp);
        AddExpAST *addexp1=new AddExpAST();
        addexp1->mulexp=std::unique_ptr<BaseAST>(mulexp1);
        RelExpAST *relexp1=new RelExpAST();
        relexp1->addexp=std::unique_ptr<BaseAST>(addexp1);
        EqExpAST *eqexp1=new EqExpAST();
        eqexp1->relexp=std::unique_ptr<BaseAST>(relexp1);
        LAndExpAST *landexp1=new LAndExpAST();
        landexp1->eqexp=std::unique_ptr<BaseAST>(eqexp1);
        LOrExpAST *lorexp1=new LOrExpAST();
        lorexp1->landexp=std::unique_ptr<BaseAST>(landexp1);
        ExpAST *exp=new ExpAST();
        exp->lorexp=std::unique_ptr<BaseAST>(lorexp1);

        LOrExpAST *lorexp3 = new LOrExpAST();
        lorexp3->landexp = std::unique_ptr<BaseAST>(landexp.get());
        ExpAST *exp3 = new ExpAST();
        exp3->lorexp = std::unique_ptr<BaseAST>(lorexp3);
        LValAST* lval3=new LValAST();
        lval3->ident=ident;
        Stmt_withoutIFAST* other3=new Stmt_withoutIFAST();
        other3->lval=std::unique_ptr<BaseAST>(lval3);
        other3->exp=std::unique_ptr<BaseAST>(exp3);

        Open_StmtAST *os = new Open_StmtAST();
        os->exp = std::unique_ptr<BaseAST>(exp);;
        os->then_stmt = std::unique_ptr<BaseAST>(other3);
        bool dump_arg = false;
        os->Dump(dump_arg);

        // result=result==1
        LValAST* lval4=new LValAST();
        lval4->ident=ident;
        PrimaryExpAST *result=new PrimaryExpAST();
        result->lval=std::unique_ptr<BaseAST>(lval4);
        UnaryExpAST *unary4 = new UnaryExpAST();
        unary4->primaryexp = std::unique_ptr<BaseAST>(result);
        MulExpAST *mulexp4 = new MulExpAST();
        mulexp4->unaryexp = std::unique_ptr<BaseAST>(unary4);
        AddExpAST *addexp4 = new AddExpAST();
        addexp4->mulexp = std::unique_ptr<BaseAST>(mulexp4);
        RelExpAST *relexp4 = new RelExpAST();
        relexp4->addexp = std::unique_ptr<BaseAST>(addexp4);
        EqExpAST *eqexp4 = new EqExpAST();
        eqexp4->relexp = std::unique_ptr<BaseAST>(relexp4);
        /**/
        EqExpAST *eqexp5=new EqExpAST();  // result!=0
        eqexp5->eqexp=std::unique_ptr<BaseAST>(eqexp4);  // result
        eqexp5->op=std::string("==");
        /**/
        PrimaryExpAST *one2 = new PrimaryExpAST();
        one2->number = 1;
        UnaryExpAST *unary6 = new UnaryExpAST();
        unary6->primaryexp = std::unique_ptr<BaseAST>(one2);
        MulExpAST *mulexp6 = new MulExpAST();
        mulexp6->unaryexp = std::unique_ptr<BaseAST>(unary6);
        AddExpAST *addexp6 = new AddExpAST();
        addexp6->mulexp = std::unique_ptr<BaseAST>(mulexp6);
        RelExpAST *relexp6 = new RelExpAST();
        relexp6->addexp = std::unique_ptr<BaseAST>(addexp6);
        /**/
        eqexp5->relexp=std::unique_ptr<BaseAST>(relexp6); // one
        LAndExpAST *landexp5 = new LAndExpAST();
        landexp5->eqexp = std::unique_ptr<BaseAST>(eqexp5);
        LOrExpAST *lorexp5 = new LOrExpAST();
        lorexp5->landexp = std::unique_ptr<BaseAST>(landexp5);
        ExpAST *exp5 = new ExpAST();
        exp5->lorexp = std::unique_ptr<BaseAST>(lorexp5);
        LValAST* lval5=new LValAST();
        lval5->ident=ident;
        Stmt_withoutIFAST* other5=new Stmt_withoutIFAST();
        other5->lval=std::unique_ptr<BaseAST>(lval5);
        other5->exp=std::unique_ptr<BaseAST>(exp5);
        other5->Dump(dump_arg);

        res=std::string("variable");
    }
}

void load_libfunc() {
    std::cout<<"decl @getint(): i32"<<std::endl;
    std::cout<<"decl @getch(): i32"<<std::endl;
    std::cout<<"decl @getarray(*i32): i32"<<std::endl;
    std::cout<<"decl @putint(i32)"<<std::endl;
    std::cout<<"decl @putch(i32)"<<std::endl;
    std::cout<<"decl @putarray(i32, *i32)"<<std::endl;
    std::cout<<"decl @starttime()"<<std::endl;
    std::cout<<"decl @stoptime()"<<std::endl;
    std::string int_type="int";
    std::string void_type="void";
    BaseAST::top->insert(std::string("getint()"),var(int_type));
    BaseAST::top->insert(std::string("getch()"),var(int_type));
    BaseAST::top->insert(std::string("getarray()"),var(int_type));
    BaseAST::top->insert(std::string("putint()"),var(void_type));
    BaseAST::top->insert(std::string("putch()"),var(void_type));
    BaseAST::top->insert(std::string("putarray()"),var(void_type));
    BaseAST::top->insert(std::string("starttime()"),var(void_type));
    BaseAST::top->insert(std::string("stoptime()"),var(void_type));
}