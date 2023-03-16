//
// Created by sakura on 2022/10/9.
//

#include "symTable.h"
void symTable::insert(std::string ident, var value) {
    table.insert(std::make_pair<std::string, var>(std::move(ident),std::move(value)));
}
bool symTable::contain(std::string ident) {
    for(symTable* t=this;t!=NULL;t=t->prev) {
        bool found=(bool)t->table.count(ident);
        if(found) return true;
    }
    return false;
}
var symTable::getValue(std::string ident) {
    for(symTable* t=this;t!=NULL;t=t->prev) {
        auto found=t->table.find(ident);
        if(found!=t->table.end()) return (*found).second;
    }
    var err;
    return err;  // 没找到返回空值
}
symTable::symTable() {
    table=std::unordered_map<std::string, var>();
    prev=NULL;
}
symTable::symTable(symTable* p) {
    table=std::unordered_map<std::string, var>();
    prev=p;
}