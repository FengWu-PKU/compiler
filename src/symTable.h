//
// Created by sakura on 2022/10/9.
//

#ifndef COMPILER_SYMTABLE_H
#define COMPILER_SYMTABLE_H
#include <unordered_map>
#include <variant>
typedef std::variant<int,std::string> var;
class symTable {
private:
    std::unordered_map<std::string, var> table;
    symTable* prev;
public:
    symTable();
    symTable(symTable* p);
    void insert(std::string ident, var value);
    bool contain(std::string ident);
    var getValue(std::string ident);

};


#endif //COMPILER_SYMTABLE_H
