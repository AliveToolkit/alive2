#pragma once
#include <iostream>
#include "instructions.h"
#include "parser.h"
#include <fstream>
#include <map>
#include "singleLine.h"
#include <unordered_map>

/*
 * This class is used for doing simple mutations on a given file.
 Current supported opeartions on binary instructions:
    + swap operands
    + replacing operator
    + reset math flag
    + replace constant
 On GEP instructions:
    + reset inbounds flag
 On function definitions:
    + reset nofree flag
 On function parameters:
    + reset dereferenceable flag with random value (garantee is power and 2 and value range is [1,8])
    + reset nocapture flag
*/
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