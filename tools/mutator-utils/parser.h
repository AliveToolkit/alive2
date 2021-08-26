#pragma once
#include <string>
#include <algorithm>
#include <vector>
#include <memory>
#include "instructions.h"
#include <cctype>
#include <functional>
#include "util.h"
#include "attributes.h"
#include "singleLine.h"
#include <deque>

using std::unique_ptr;

class Parser{
    bool isNamedValues(const string& str);
    bool isUnnamedValues(const string& str);
    bool isConstant(const string& str);
    bool isIdentifier(const string& str);  
    bool isSupportBinaryOperator(const string& str);
    bool isFunctionGroupID(const string& str);
    bool isUnnamedAddr(const string& str);
    bool isAddrSpace(const string& str);
    unique_ptr<BinaryInstruction> parseBinaryInstruction(const vector<string>& tokens,const string& comment);
    unique_ptr<GEPInstruction> parseGEPInstruction(const vector<string>& tokens,const string& comment);
    unique_ptr<Instruction> parseInstruction(const vector<string>& tokens, const string& comment);
    unique_ptr<FunctionAttributeGroupDefinition> parseFunctionAttributeGroupDefinition(const vector<string>& tokens, const string& comment);
    unique_ptr<Parameter> parseParameter(const string& str);
    unique_ptr<ParameterAttribute> parseParameterAttribute(const string& str);

    std::tuple<string,string,string,string> getFunctionNameAndParameter(const string& str);
    std::vector<unique_ptr<Parameter>> getFunctionParameter(const string& str);
    std::tuple<string,std::vector<std::unique_ptr<FunctionAttribute>>,string> getFunctionAttribute(const string& str);
public:
    unique_ptr<BinaryInstruction> parseBinaryInstruction(const std::string& str);
    unique_ptr<GEPInstruction> parseGEPInstruction(const std::string& str);
    unique_ptr<Instruction> parseInstruction(const std::string& str);
    unique_ptr<FunctionAttribute> parseFunctionAttribute(const std::string& str);
    unique_ptr<FunctionAttributeGroupDefinition> parseFunctionAttributeGroupDefinition(const string& str);
    unique_ptr<FunctionDefinition> parseFunctionDefinition(const string& str,const string& comment);
    unique_ptr<SingleLine> parseSinleLine(const std::string& str);
    std::pair<string,string> removeComment(const std::string& str);
};