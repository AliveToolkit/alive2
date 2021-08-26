#include "instructions.h"

const std::vector<std::string> operFlags[]{std::vector<std::string>(),
                                        std::vector<std::string>{"nuw","nsw"},
                                        std::vector<std::string>{"nnan","ninf","nsz","arcp","contract","afn","reassoc"},
                                        std::vector<std::string>{"exact"},
                                        std::vector<std::string>{"nuw","nsw"},
                                        std::vector<std::string>{"exact"},
                                        std::vector<std::string>()};

const std::unordered_map<string,int> operToFlagIndex({{"urem",0},{"srem",0},
                                                    {"add",1},{"sub",1},{"mul",1},
                                                    {"fadd",2},{"fsub",2},{"fmul",2},{"fdiv",2},{"frem",2},
                                                    {"udiv",3},{"sdiv",3},
                                                    {"shl",4},
                                                    {"lshr",5},{"ashr",5},
                                                    {"and",6},{"or",6},{"xor",6}});


const vector<vector<string>> FlagIndexToOper({{"urem","srem"},
                                            {"add","sub","mul"},
                                            {"fadd","fsub","fmul","fdiv","frem"},
                                            {"udiv","sdiv"},
                                            {"shl"},
                                            {"lshr","ashr"},
                                            {"and","or","xor"}});

const string GEPInstruction::INSTRUCTION="getelementptr";
const string GEPInstruction::FLAG="inbounds";
