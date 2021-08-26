#include "attributes.h"
#include "singleLine.h"
bool FunctionAttributeGroup::hasSupportedFunctionAttribute()const{
    if(auto it= FunctionAttributeGroupDefinition::groupAttrs.find(name);it!=FunctionAttributeGroupDefinition::groupAttrs.end())
        return it->second.first;
    return false;
}