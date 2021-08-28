#pragma once
#include <iostream>
#include "instructions.h"
#include "parser.h"
#include <fstream>
#include <map>
#include "singleLine.h"
#include <unordered_map>


class SingleLineMutator{
    std::map<int,unique_ptr<Instruction>> updPos;
    std::map<int,unique_ptr<FunctionDefinition>> funcDefPos;
    std::map<int,unique_ptr<FunctionAttributeGroupDefinition>> updFuncGroupAttrDef;
    decltype(updPos.begin()) cur;
    decltype(funcDefPos.begin()) funcCur;
    std::vector<string> testFile;
    bool debug;
    bool changed,funcChanged;
    std::unordered_set<string> funcGroupAttrNames;
    const static std::string INST_INDENT;
public:
    SingleLineMutator():debug(false),changed(false),funcChanged(false){};
    ~SingleLineMutator(){};
    bool openInputFile(const string& inputFile);
    bool init();
    void generateTest(const string& outputFileName);
    void setDebug(bool debug){this->debug=debug;}


};