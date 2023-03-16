%code requires {
  #include <memory>
  #include <string>
  #include "AST.h"
  #include <vector>
}  // %union中需要用到的

%{

#include <iostream>
#include <memory>
#include <string>
#include "AST.h"
#include <vector>

// 声明 lexer 函数和错误处理函数
int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;


%}

// 定义 parser 函数和错误处理函数的附加参数
// 我们需要返回一个字符串作为 AST, 所以我们把附加参数定义成字符串的智能指针
// 解析完成后, 我们要手动修改这个参数, 把它设置成解析得到的字符串
%parse-param { std::unique_ptr<BaseAST> &ast1 }

// yylval 的定义, 我们把它定义成了一个联合体 (union)
// 因为 token 的值有的是字符串指针, 有的是整数
// 之前我们在 lexer 中用到的 str_val 和 int_val 就是在这里被定义的
// 至于为什么要用字符串指针而不直接用 string 或者 unique_ptr<string>?
// 请自行 STFW 在 union 里写一个带析构函数的类会出现什么情况
%union {
  std::string *str_val;
  int int_val;
  BaseAST *ast_val;
  std::vector<std::unique_ptr<BaseAST>> *vec_val;
}

// lexer 返回的所有 token 种类的声明
// 注意 IDENT 和 INT_CONST 会返回 token 的值, 分别对应 str_val 和 int_val
%token INT VOID RETURN CONST IF ELSE WHILE BREAK CONTINUE
%token <str_val> IDENT
%token <int_val> INT_CONST
%token MINUS PLUS LOGREVERSE
%token MULTIPLE DIVIDE MOD
%token GREATERTHAN GREATEREQUALTHAN LESSTHAN LESSEQUALTHAN EQUAL NOTEQUAL AND OR

// 非终结符的类型定义
%type <ast_val> FuncDef Block Stmt_withoutIF Stmt
%type <ast_val> Exp Exp_OneOrNone PrimaryExp UnaryExp MulExp AddExp RelExp EqExp LAndExp LOrExp
%type <int_val> Number
%type <ast_val> Decl ConstDecl ConstDef ConstInitVal ConstExp VarDecl VarDef InitVal FuncFParam
%type <ast_val> LVal
%type <vec_val> ConstDef_OP BlockItem VarDef_OP CompUnit_OP FuncFParams FuncFParams_OP FuncRParams FuncRParams_OP
%type <ast_val> Open_Stmt Matched_Stmt While_Stmt
%type <vec_val> ArrIndex ExpIndex ConstArrInit ArrInit

%%

// 开始符, CompUnit ::= FuncDef, 大括号后声明了解析完成后 parser 要做的事情
// 之前我们定义了 FuncDef 会返回一个 str_val, 也就是字符串指针
// 而 parser 一旦解析完 CompUnit, 就说明所有的 token 都被解析了, 即解析结束了
// 此时我们应该把 FuncDef 返回的结果收集起来, 作为 AST 传给调用 parser 的函数
// $1 指代规则里第一个符号的返回值, 也就是 FuncDef 的返回值
CompUnit
  : FuncDef CompUnit_OP {
    auto comp_unit=make_unique<CompUnitAST>();
    comp_unit->defs=unique_ptr<vector<unique_ptr<BaseAST>>>(new vector<unique_ptr<BaseAST>>);
    (comp_unit->defs)->push_back(unique_ptr<BaseAST>($1));
    auto other=$2;
    for(auto it=other->begin();it!=other->end();it++) {
        (comp_unit->defs)->push_back(move(*it));
    }
    ast1=move(comp_unit);
  }
  | Decl CompUnit_OP {
    auto comp_unit=make_unique<CompUnitAST>();
    comp_unit->defs=unique_ptr<vector<unique_ptr<BaseAST>>>(new vector<unique_ptr<BaseAST>>);
    (comp_unit->defs)->push_back(unique_ptr<BaseAST>($1));
    auto other=$2;
    for(auto it=other->begin();it!=other->end();it++) {
        (comp_unit->defs)->push_back(move(*it));
    }
    ast1=move(comp_unit);
  }
  ;

CompUnit_OP
  : FuncDef CompUnit_OP {
    auto vec=new vector<unique_ptr<BaseAST>>();
    vec->push_back(unique_ptr<BaseAST>($1));
    auto op=$2;
    for(auto it=op->begin();it!=op->end();it++) {
        vec->push_back(move(*it));
    }
    $$=vec;
  }
  | Decl CompUnit_OP {
    auto vec=new vector<unique_ptr<BaseAST>>();
    vec->push_back(unique_ptr<BaseAST>($1));
    auto op=$2;
    for(auto it=op->begin();it!=op->end();it++) {
      vec->push_back(move(*it));
    }
    $$=vec;
    }
  | %empty { $$=new vector<unique_ptr<BaseAST>>(); }
  ;

// FuncDef ::= FuncType IDENT '(' ')' Block;
// 我们这里可以直接写 '(' 和 ')', 因为之前在 lexer 里已经处理了单个字符的情况
// 解析完成后, 把这些符号的结果收集起来, 然后拼成一个新的字符串, 作为结果返回
// $$ 表示非终结符的返回值, 我们可以通过给这个符号赋值的方法来返回结果
// 你可能会问, FuncType, IDENT 之类的结果已经是字符串指针了
// 为什么还要用 unique_ptr 接住它们, 然后再解引用, 把它们拼成另一个字符串指针呢
// 因为所有的字符串指针都是我们 new 出来的, new 出来的内存一定要 delete
// 否则会发生内存泄漏, 而 unique_ptr 这种智能指针可以自动帮我们 delete
// 虽然此处你看不出用 unique_ptr 和手动 delete 的区别, 但当我们定义了 AST 之后
// 这种写法会省下很多内存管理的负担
FuncDef
  : INT IDENT '(' FuncFParams ')' Block {
    auto ast2 =new FuncDefAST();
    ast2->func_type=*unique_ptr<string>(new string("int"));
    ast2->ident=*unique_ptr<string>($2);
    ast2->params=unique_ptr<vector<unique_ptr<BaseAST>>>($4);
    ast2->block=unique_ptr<BaseAST>($6);
    $$=ast2;
  }
  | VOID IDENT '(' FuncFParams ')' Block {
    auto ast2 =new FuncDefAST();
    ast2->func_type=*unique_ptr<string>(new string("void"));
    ast2->ident=*unique_ptr<string>($2);
    ast2->params=unique_ptr<vector<unique_ptr<BaseAST>>>($4);
    ast2->block=unique_ptr<BaseAST>($6);
    $$=ast2;
  }
  ;

FuncFParams
  : FuncFParam FuncFParams_OP {
    auto vec=new vector<unique_ptr<BaseAST>>();
    vec->push_back(unique_ptr<BaseAST>($1));
    auto op=$2;
    for(auto it=op->begin();it!=op->end();it++) {
      vec->push_back(move(*it));
    }
    $$=vec;
  }
  | %empty {
    $$=new vector<unique_ptr<BaseAST>>();
  }
  ;


FuncFParams_OP
  : ',' FuncFParam FuncFParams_OP
  { auto vec=new vector<unique_ptr<BaseAST>>();
    vec->push_back(unique_ptr<BaseAST>($2));
    auto op=$3;
    for(auto it=op->begin();it!=op->end();it++) {
      vec->push_back(move(*it));
    }
    $$=vec;
  }
  | %empty { $$=new vector<unique_ptr<BaseAST>>(); }
  ;

FuncFParam
  : INT IDENT {
    auto ast2=new FuncFParamAST();
    ast2->btype=*unique_ptr<string>(new string("int"));
    ast2->ident=*unique_ptr<string>($2);
    ast2->indexes=NULL;
    $$=ast2;
  }
  | INT IDENT '[' ']' ArrIndex {
    auto ast2=new FuncFParamAST();
    ast2->btype=*unique_ptr<string>(new string("int"));
    ast2->ident=*unique_ptr<string>($2);
    ast2->indexes=unique_ptr<vector<unique_ptr<BaseAST>>>($5);
    $$=ast2;
  }
  ;



Block
  : '{' BlockItem '}' {
    auto ast2=new BlockAST();
    ast2->blockitem=unique_ptr<vector<unique_ptr<BaseAST>>>($2);
    $$=ast2;
  }
  ;

BlockItem
  : Decl BlockItem
    { auto vec=new vector<unique_ptr<BaseAST>>();
      vec->push_back(unique_ptr<BaseAST>($1));
      auto op=$2;
      for(auto it=op->begin();it!=op->end();it++) {
        vec->push_back(move(*it));
      }
      $$=vec;
    }
  | Stmt BlockItem
    { auto vec=new vector<unique_ptr<BaseAST>>();
      vec->push_back(unique_ptr<BaseAST>($1));
      auto op=$2;
      for(auto it=op->begin();it!=op->end();it++) {
        vec->push_back(move(*it));
      }
      $$=vec;
    }
  | %empty { $$=new vector<unique_ptr<BaseAST>>();}
  ;

While_Stmt
  : WHILE '(' Exp ')' Stmt {
    auto ast2=new While_StmtAST();
    ast2->exp=unique_ptr<BaseAST>($3);
    ast2->stmt=unique_ptr<BaseAST>($5);
    $$=ast2;
  }

Stmt_withoutIF
  : RETURN Exp_OneOrNone ';' {
    auto ast2=new Stmt_withoutIFAST();
    ast2->ret=true;
    ast2->exp=unique_ptr<BaseAST>($2);
    $$ = ast2;
  }
  | LVal '=' Exp ';' {
    auto ast2=new Stmt_withoutIFAST();
    ast2->lval=unique_ptr<BaseAST>($1);
    ast2->exp=unique_ptr<BaseAST>($3);
    $$ = ast2;
  }
  | Exp_OneOrNone ';' {
    auto ast2=new Stmt_withoutIFAST();
    ast2->exp=unique_ptr<BaseAST>($1);
    $$ = ast2;
  }
  | Block {
    auto ast2=new Stmt_withoutIFAST();
    ast2->block=unique_ptr<BaseAST>($1);
    $$=ast2;
  }
  | While_Stmt {
    auto ast2=new Stmt_withoutIFAST();
    ast2->while_stmt=unique_ptr<BaseAST>($1);
    $$=ast2;
  }
  | BREAK  ';' {
    auto ast2=new Stmt_withoutIFAST();
    ast2->Break=unique_ptr<BaseAST>(new BreakAST());
    $$=ast2;
  }
  | CONTINUE ';'  {
    auto ast2=new Stmt_withoutIFAST();
    ast2->Continue=unique_ptr<BaseAST>(new ContinueAST());
    $$=ast2;
  }
  ;


Matched_Stmt
  : IF '(' Exp ')' Matched_Stmt ELSE Matched_Stmt {
    auto ast2=new Matched_StmtAST();
    ast2->exp=unique_ptr<BaseAST>($3);
    ast2->then_stmt=unique_ptr<BaseAST>($5);
    ast2->else_stmt=unique_ptr<BaseAST>($7);
    $$=ast2;
  }
  | Stmt_withoutIF {
    auto ast2=new Matched_StmtAST();
    ast2->other=unique_ptr<BaseAST>($1);
    $$=ast2;
  }
  ;

Open_Stmt
  : IF '(' Exp ')' Stmt {
    auto ast2=new Open_StmtAST();
    ast2->exp=unique_ptr<BaseAST>($3);
    ast2->then_stmt=unique_ptr<BaseAST>($5);
    $$=ast2;
  }
  | IF '(' Exp ')' Matched_Stmt ELSE Open_Stmt {
    auto ast2=new Open_StmtAST();
    ast2->exp=unique_ptr<BaseAST>($3);
    ast2->then_stmt=unique_ptr<BaseAST>($5);
    ast2->else_stmt=unique_ptr<BaseAST>($7);
    $$=ast2;
  }
  ;

Stmt
  : Matched_Stmt {
    auto ast2=new StmtAST();
    ast2->matched=unique_ptr<BaseAST>($1);
    $$=ast2;
  }
  | Open_Stmt {
    auto ast2=new StmtAST();
    ast2->open=unique_ptr<BaseAST>($1);
    $$=ast2;
  }
  ;


Number
  : INT_CONST {
    $$=int($1);
  }
  ;
  ;

/* 一元表达式 */
Exp
  : LOrExp {
    auto ast2=new ExpAST();
    ast2->lorexp=unique_ptr<BaseAST>($1);
    $$=ast2;
  }
  ;

Exp_OneOrNone
  : Exp {
    $$=$1;
  }
  | %empty {
    $$=NULL;
  }
  ;


PrimaryExp
  : '(' Exp ')'
    { auto ast2=new PrimaryExpAST();
      ast2->exp=unique_ptr<BaseAST>($2);
      $$=ast2;
    }
  | Number
    { auto ast2=new PrimaryExpAST();
      ast2->number=int($1);
      $$=ast2;
    }
  | LVal
    { auto ast2=new PrimaryExpAST();
      ast2->lval=unique_ptr<BaseAST>($1);
      $$=ast2;
    }
  ;

UnaryExp
  : PrimaryExp
    { auto ast2=new UnaryExpAST();
      ast2->primaryexp=unique_ptr<BaseAST>($1);
      $$=ast2;
    }
  | PLUS UnaryExp
    { auto ast2=new UnaryExpAST();
      ast2->unaryop=*unique_ptr<string>(new string("+"));
      ast2->unaryexp=unique_ptr<BaseAST>($2);
      $$=ast2;
    }
  | MINUS UnaryExp
    { auto ast2=new UnaryExpAST();
    ast2->unaryop=*unique_ptr<string>(new string("-"));
    ast2->unaryexp=unique_ptr<BaseAST>($2);
    $$=ast2;
    }
  | LOGREVERSE UnaryExp
    { auto ast2=new UnaryExpAST();
    ast2->unaryop=*unique_ptr<string>(new string("!"));
    ast2->unaryexp=unique_ptr<BaseAST>($2);
    $$=ast2;
    }
  | IDENT '(' FuncRParams ')' {
    auto ast2=new UnaryExpAST();
    ast2->ident=*unique_ptr<string>($1);
    ast2->params=unique_ptr<vector<unique_ptr<BaseAST>>>($3);
    $$=ast2;
  }
  ;

FuncRParams
  : Exp FuncRParams_OP {
    auto vec=new vector<unique_ptr<BaseAST>>();
    vec->push_back(unique_ptr<BaseAST>($1));
    auto op=$2;
    for(auto it=op->begin();it!=op->end();it++) {
      vec->push_back(move(*it));
    }
    $$=vec;
  }
  | %empty {
    $$=new vector<unique_ptr<BaseAST>>();
  }
  ;


FuncRParams_OP
  : ',' Exp FuncRParams_OP
  { auto vec=new vector<unique_ptr<BaseAST>>();
    vec->push_back(unique_ptr<BaseAST>($2));
    auto op=$3;
    for(auto it=op->begin();it!=op->end();it++) {
      vec->push_back(move(*it));
    }
    $$=vec;
  }
  | %empty { $$=new vector<unique_ptr<BaseAST>>(); }
  ;

MulExp
  : UnaryExp
    { auto ast2=new MulExpAST();
      ast2->unaryexp=unique_ptr<BaseAST>($1);
      $$=ast2;
    }
  | MulExp MULTIPLE UnaryExp
    { auto ast2=new MulExpAST();
      ast2->mulexp=unique_ptr<BaseAST>($1);
      ast2->op=*unique_ptr<string>(new string("*"));
      ast2->unaryexp=unique_ptr<BaseAST>($3);
      $$=ast2;
    }
  | MulExp DIVIDE UnaryExp
    { auto ast2=new MulExpAST();
    ast2->mulexp=unique_ptr<BaseAST>($1);
    ast2->op=*unique_ptr<string>(new string("/"));
    ast2->unaryexp=unique_ptr<BaseAST>($3);
    $$=ast2;
    }
  | MulExp MOD UnaryExp
    { auto ast2=new MulExpAST();
    ast2->mulexp=unique_ptr<BaseAST>($1);
    ast2->op=*unique_ptr<string>(new string("%"));
    ast2->unaryexp=unique_ptr<BaseAST>($3);
    $$=ast2;
    }
  ;

AddExp
  : MulExp
    { auto ast2=new AddExpAST();
      ast2->mulexp=unique_ptr<BaseAST>($1);
      $$=ast2;
    }
  | AddExp PLUS MulExp
    { auto ast2=new AddExpAST();
      ast2->addexp=unique_ptr<BaseAST>($1);
      ast2->op=*unique_ptr<string>(new string("+"));
      ast2->mulexp=unique_ptr<BaseAST>($3);
      $$=ast2;
    }
  | AddExp MINUS MulExp
    { auto ast2=new AddExpAST();
    ast2->addexp=unique_ptr<BaseAST>($1);
    ast2->op=*unique_ptr<string>(new string("-"));
    ast2->mulexp=unique_ptr<BaseAST>($3);
    $$=ast2;
    }
  ;

RelExp
  : AddExp
    { auto ast2=new RelExpAST();
      ast2->addexp=unique_ptr<BaseAST>($1);
      $$=ast2;
    }
  | RelExp LESSTHAN AddExp
    { auto ast2=new RelExpAST();
      ast2->relexp=unique_ptr<BaseAST>($1);
      ast2->op=*unique_ptr<string>(new string("<"));
      ast2->addexp=unique_ptr<BaseAST>($3);
      $$=ast2;
    }
  | RelExp GREATERTHAN AddExp
    { auto ast2=new RelExpAST();
    ast2->relexp=unique_ptr<BaseAST>($1);
    ast2->op=*unique_ptr<string>(new string(">"));
    ast2->addexp=unique_ptr<BaseAST>($3);
    $$=ast2;
    }
  | RelExp LESSEQUALTHAN AddExp
    { auto ast2=new RelExpAST();
    ast2->relexp=unique_ptr<BaseAST>($1);
    ast2->op=*unique_ptr<string>(new string("<="));
    ast2->addexp=unique_ptr<BaseAST>($3);
    $$=ast2;
    }
  | RelExp GREATEREQUALTHAN AddExp
    { auto ast2=new RelExpAST();
    ast2->relexp=unique_ptr<BaseAST>($1);
    ast2->op=*unique_ptr<string>(new string(">="));
    ast2->addexp=unique_ptr<BaseAST>($3);
    $$=ast2;
    }
  ;

EqExp
  : RelExp
    { auto ast2=new EqExpAST();
      ast2->relexp=unique_ptr<BaseAST>($1);
      $$=ast2;
    }
  | EqExp EQUAL RelExp
    { auto ast2=new EqExpAST();
      ast2->eqexp=unique_ptr<BaseAST>($1);
      ast2->op=*unique_ptr<string>(new string("=="));
      ast2->relexp=unique_ptr<BaseAST>($3);
      $$=ast2;
    }
  | EqExp NOTEQUAL RelExp
    { auto ast2=new EqExpAST();
    ast2->eqexp=unique_ptr<BaseAST>($1);
    ast2->op=*unique_ptr<string>(new string("!="));
    ast2->relexp=unique_ptr<BaseAST>($3);
    $$=ast2;
    }
  ;

LAndExp
  : EqExp
    { auto ast2=new LAndExpAST();
      ast2->eqexp=unique_ptr<BaseAST>($1);
      $$=ast2;
    }
  | LAndExp AND EqExp
    { auto ast2=new LAndExpAST();
      ast2->landexp=unique_ptr<BaseAST>($1);
      ast2->op=*unique_ptr<string>(new string("&&"));
      ast2->eqexp=unique_ptr<BaseAST>($3);
      $$=ast2;
    }
  ;

LOrExp
  : LAndExp
    { auto ast2=new LOrExpAST();
      ast2->landexp=unique_ptr<BaseAST>($1);
      $$=ast2;
    }
  | LOrExp OR LAndExp
    { auto ast2=new LOrExpAST();
      ast2->lorexp=unique_ptr<BaseAST>($1);
      ast2->op=*unique_ptr<string>(new string("||"));
      ast2->landexp=unique_ptr<BaseAST>($3);
      $$=ast2;
    }
  ;

Decl
  : ConstDecl {
    auto ast2=new DeclAST();
    ast2->constdecl=unique_ptr<BaseAST>($1);
    $$=ast2;
  }
  | VarDecl {
    auto ast2=new DeclAST();
    ast2->vardecl=unique_ptr<BaseAST>($1);
    $$=ast2;
  }
  ;

ConstDecl
  : CONST INT ConstDef ConstDef_OP ';' {
    auto ast2=new ConstDeclAST();
    ast2->btype=*unique_ptr<string>(new string("int"));
    ast2->constdef=unique_ptr<BaseAST>($3);
    ast2->constdef_op=unique_ptr<vector<unique_ptr<BaseAST>>>($4);
    $$=ast2;
  }
  ;

ConstDef_OP
  : ',' ConstDef ConstDef_OP
    { auto vec=new vector<unique_ptr<BaseAST>>();
      vec->push_back(unique_ptr<BaseAST>($2));
      auto op=$3;
      for(auto it=op->begin();it!=op->end();it++) {
        vec->push_back(move(*it));
      }
      $$=vec;
    }
  | %empty { $$=new vector<unique_ptr<BaseAST>>(); }
  ;

ArrIndex
  : '[' ConstExp ']' ArrIndex {
    auto vec=new vector<unique_ptr<BaseAST>>();
    vec->push_back(unique_ptr<BaseAST>($2));
    auto op=$4;
    for(auto it=op->begin();it!=op->end();it++) {
        vec->push_back(move(*it));
    }
    $$=vec;
  }
  | %empty { $$=new vector<unique_ptr<BaseAST>>(); }
  ;


ExpIndex
  : '[' Exp ']' ExpIndex {
    auto vec=new vector<unique_ptr<BaseAST>>();
    vec->push_back(unique_ptr<BaseAST>($2));
    auto op=$4;
    for(auto it=op->begin();it!=op->end();it++) {
        vec->push_back(move(*it));
    }
    $$=vec;
  }
  | %empty { $$=new vector<unique_ptr<BaseAST>>(); }
  ;

ConstDef
  : IDENT ArrIndex '=' ConstInitVal {
    auto ast2=new ConstDefAST();
    ast2->ident=*unique_ptr<string>($1);
    ast2->indexes=unique_ptr<vector<unique_ptr<BaseAST>>>($2);
    ast2->constinitval=unique_ptr<BaseAST>($4);
    $$=ast2;
  }
  ;

ConstArrInit
  : ',' ConstInitVal ConstArrInit {
    auto vec=new vector<unique_ptr<BaseAST>>();
    vec->push_back(unique_ptr<BaseAST>($2));
    auto op=$3;
    for(auto it=op->begin();it!=op->end();it++) {
        vec->push_back(move(*it));
    }
    $$=vec;
  }
  | %empty { $$=new vector<unique_ptr<BaseAST>>(); }
  ;

ConstInitVal
  : ConstExp {
    auto ast2=new ConstInitValAST();
    ast2->constexp=unique_ptr<BaseAST>($1);
    $$=ast2;
  }
  | '{' ConstInitVal ConstArrInit '}' {
      auto ast2=new ConstInitValAST();
      ast2->arrinit=unique_ptr<vector<unique_ptr<BaseAST>>>(new vector<unique_ptr<BaseAST>>);
      (ast2->arrinit)->push_back(unique_ptr<BaseAST>($2));
      auto op=$3;
      for(auto it=op->begin();it!=op->end();it++) {
          (ast2->arrinit)->push_back(move(*it));
      }
      $$=ast2;
  }
  | '{' '}' {
     auto ast2=new ConstInitValAST();
     ast2->arrinit=unique_ptr<vector<unique_ptr<BaseAST>>>(new vector<unique_ptr<BaseAST>>);
     $$=ast2;
  }
  ;

LVal
  : IDENT ExpIndex {
    auto ast2=new LValAST();
    ast2->ident=*unique_ptr<string>($1);
    ast2->indexes=unique_ptr<vector<unique_ptr<BaseAST>>>($2);
    $$=ast2;
  }
  ;

ConstExp
  : Exp {
    auto ast2=new ConstExpAST();
    ast2->exp=unique_ptr<BaseAST>($1);
    $$=ast2;
  }
  ;

VarDecl
  : INT VarDef VarDef_OP ';' {
    auto ast2=new VarDeclAST();
    ast2->btype=*unique_ptr<string>(new string("int"));
    ast2->vardef=unique_ptr<BaseAST>($2);
    ast2->vardef_op=unique_ptr<vector<unique_ptr<BaseAST>>>($3);
    $$=ast2;
  }
  ;

VarDef_OP
  : ',' VarDef VarDef_OP {
    auto vec=new vector<unique_ptr<BaseAST>>();
    vec->push_back(unique_ptr<BaseAST>($2));
    auto op=$3;
    for(auto it=op->begin();it!=op->end();it++) {
        vec->push_back(move(*it));
    }
    $$=vec;
  }
  | %empty {
    $$=new vector<unique_ptr<BaseAST>>();
  }
  ;

VarDef
  : IDENT ArrIndex '=' InitVal {
    auto ast2=new VarDefAST();
    ast2->ident=*unique_ptr<string>($1);
    ast2->indexes=unique_ptr<vector<unique_ptr<BaseAST>>>($2);
    ast2->initval=unique_ptr<BaseAST>($4);
    $$=ast2;
  }
  | IDENT ArrIndex {
    auto ast2=new VarDefAST();
    ast2->ident=*unique_ptr<string>($1);
    ast2->indexes=unique_ptr<vector<unique_ptr<BaseAST>>>($2);
    $$=ast2;
  }
  ;

ArrInit
  : ',' InitVal ArrInit {
    auto vec=new vector<unique_ptr<BaseAST>>();
    vec->push_back(unique_ptr<BaseAST>($2));
    auto op=$3;
    for(auto it=op->begin();it!=op->end();it++) {
        vec->push_back(move(*it));
    }
    $$=vec;
  }
  | %empty { $$=new vector<unique_ptr<BaseAST>>(); }
  ;

InitVal
  : Exp {
    auto ast2=new InitValAST();
    ast2->exp=unique_ptr<BaseAST>($1);
    $$=ast2;
  }
  | '{' InitVal ArrInit '}' {
    auto ast2=new InitValAST();
    ast2->arrinit=unique_ptr<vector<unique_ptr<BaseAST>>>(new vector<unique_ptr<BaseAST>>);
    (ast2->arrinit)->push_back(unique_ptr<BaseAST>($2));
    auto op=$3;
    for(auto it=op->begin();it!=op->end();it++) {
      (ast2->arrinit)->push_back(move(*it));
    }
    $$=ast2;
  }
  | '{' '}' {
     auto ast2=new InitValAST();
     ast2->arrinit=unique_ptr<vector<unique_ptr<BaseAST>>>(new vector<unique_ptr<BaseAST>>);
     $$=ast2;
  }
  ;

%%

// 定义错误处理函数, 其中第二个参数是错误信息
// parser 如果发生错误 (例如输入的程序出现了语法错误), 就会调用这个函数
void yyerror(std::unique_ptr<BaseAST> &ast,const char *s) {
  cerr << "error: " << s << endl;
}

