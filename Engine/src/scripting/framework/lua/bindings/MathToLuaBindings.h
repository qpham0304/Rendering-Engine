#pragma once

#include <sol/sol.hpp>
#include <string>
#include "core/features/MathIncludes.h"
#include <glm/gtx/string_cast.hpp>

class MathToLuaBindings
{
public:
    static void exportMath(sol::state& luaState);


private:
    static void _exportVectors(sol::state& luaState);
    static void _exportQuat(sol::state& luaState);
    static void _exportMat4(sol::state& luaState);
    static void _exportFunctions(sol::state& luaState);

    template<typename V>
    static void _addVectorOperators(sol::usertype<V>& ut) {
        ut[sol::meta_function::addition] = [](const V& a, const V& b) { return a + b; };
        ut[sol::meta_function::subtraction] = [](const V& a, const V& b) { return a - b; };
        ut[sol::meta_function::unary_minus] = [](const V& a) { return -a; };
        ut[sol::meta_function::equal_to] = [](const V& a, const V& b) { return a == b; };
        ut[sol::meta_function::multiplication] = sol::overload(
            [](const V& a, const V& b) { return a * b; },
            [](const V& a, float s) { return a * s; },
            [](float s, const V& a) { return s * a; }
        );
        ut[sol::meta_function::division] = sol::overload(
            [](const V& a, const V& b) { return a / b; },
            [](const V& a, float s) { return a / s; }
        );
        ut[sol::meta_function::to_string] = [](const V& v) { return glm::to_string(v); };
    }

};
