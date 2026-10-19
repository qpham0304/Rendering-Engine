#include "MathToLuaBindings.h"

void MathToLuaBindings::exportMath(sol::state &luaState)
{
    _exportVectors(luaState);
    _exportQuat(luaState);
    _exportMat4(luaState);
    _exportFunctions(luaState);
}

void MathToLuaBindings::_exportVectors(sol::state& luaState)
{
    auto vec2 = luaState.new_usertype<glm::vec2>("vec2",
        sol::call_constructor, sol::constructors<glm::vec2(), glm::vec2(float), glm::vec2(float, float)>(),
        "x", &glm::vec2::x,
        "y", &glm::vec2::y
    );
    _addVectorOperators(vec2);

    auto vec3 = luaState.new_usertype<glm::vec3>("vec3",
        sol::call_constructor, sol::constructors<glm::vec3(), glm::vec3(float), glm::vec3(float, float, float)>(),
        "x", &glm::vec3::x,
        "y", &glm::vec3::y,
        "z", &glm::vec3::z
    );
    _addVectorOperators(vec3);

    auto vec4 = luaState.new_usertype<glm::vec4>("vec4",
        sol::call_constructor, sol::constructors<
            glm::vec4(), glm::vec4(float), glm::vec4(float, float, float, float), glm::vec4(const glm::vec3&, float)
        >(),
        "x", &glm::vec4::x,
        "y", &glm::vec4::y,
        "z", &glm::vec4::z,
        "w", &glm::vec4::w
    );
    _addVectorOperators(vec4);
}

void MathToLuaBindings::_exportQuat(sol::state& luaState)
{
    luaState.new_usertype<glm::quat>("quat",
        sol::call_constructor, sol::factories(
            []() { return glm::quat(1.0f, 0.0f, 0.0f, 0.0f); },
            [](float w, float x, float y, float z) { return glm::quat(w, x, y, z); },
            [](const glm::vec3& eulerRadians) { return glm::quat(eulerRadians); }
        ),
        "w", &glm::quat::w,
        "x", &glm::quat::x,
        "y", &glm::quat::y,
        "z", &glm::quat::z,
        sol::meta_function::multiplication, sol::overload(
            [](const glm::quat& a, const glm::quat& b) { return a * b; },
            [](const glm::quat& q, const glm::vec3& v) { return q * v; },
            [](const glm::quat& q, float s) { return q * s; }
        ),
        sol::meta_function::equal_to, [](const glm::quat& a, const glm::quat& b) { return a == b; },
        sol::meta_function::to_string, [](const glm::quat& q) { return glm::to_string(q); }
    );
}

void MathToLuaBindings::_exportMat4(sol::state& luaState)
{
    luaState.new_usertype<glm::mat4>("mat4",
        sol::call_constructor, sol::factories(
            []() { return glm::mat4(1.0f); },
            [](float diagonal) { return glm::mat4(diagonal); }
        ),
        "get", [](const glm::mat4& m, int column, int row) { return m[column - 1][row - 1]; },
        "set", [](glm::mat4& m, int column, int row, float value) { m[column - 1][row - 1] = value; },
        "column", [](const glm::mat4& m, int column) { return m[column - 1]; },
        sol::meta_function::multiplication, sol::overload(
            [](const glm::mat4& a, const glm::mat4& b) { return a * b; },
            [](const glm::mat4& m, const glm::vec4& v) { return m * v; },
            [](const glm::mat4& m, float s) { return m * s; }
        ),
        sol::meta_function::equal_to, [](const glm::mat4& a, const glm::mat4& b) { return a == b; },
        sol::meta_function::to_string, [](const glm::mat4& m) { return glm::to_string(m); }
    );
}

void MathToLuaBindings::_exportFunctions(sol::state& luaState)
{
    sol::table glmTable = luaState.create_named_table("glm");

    glmTable["dot"] = sol::overload(
        [](const glm::vec2& a, const glm::vec2& b) { return glm::dot(a, b); },
        [](const glm::vec3& a, const glm::vec3& b) { return glm::dot(a, b); },
        [](const glm::vec4& a, const glm::vec4& b) { return glm::dot(a, b); },
        [](const glm::quat& a, const glm::quat& b) { return glm::dot(a, b); }
    );
    glmTable["cross"] = [](const glm::vec3& a, const glm::vec3& b) { return glm::cross(a, b); };
    glmTable["length"] = sol::overload(
        [](const glm::vec2& v) { return glm::length(v); },
        [](const glm::vec3& v) { return glm::length(v); },
        [](const glm::vec4& v) { return glm::length(v); },
        [](const glm::quat& q) { return glm::length(q); }
    );
    glmTable["distance"] = sol::overload(
        [](const glm::vec2& a, const glm::vec2& b) { return glm::distance(a, b); },
        [](const glm::vec3& a, const glm::vec3& b) { return glm::distance(a, b); },
        [](const glm::vec4& a, const glm::vec4& b) { return glm::distance(a, b); }
    );
    glmTable["normalize"] = sol::overload(
        [](const glm::vec2& v) { return glm::normalize(v); },
        [](const glm::vec3& v) { return glm::normalize(v); },
        [](const glm::vec4& v) { return glm::normalize(v); },
        [](const glm::quat& q) { return glm::normalize(q); }
    );
    glmTable["mix"] = sol::overload(
        [](float a, float b, float t) { return glm::mix(a, b, t); },
        [](const glm::vec2& a, const glm::vec2& b, float t) { return glm::mix(a, b, t); },
        [](const glm::vec3& a, const glm::vec3& b, float t) { return glm::mix(a, b, t); },
        [](const glm::vec4& a, const glm::vec4& b, float t) { return glm::mix(a, b, t); }
    );
    glmTable["clamp"] = sol::overload(
        [](float v, float lo, float hi) { return glm::clamp(v, lo, hi); },
        [](const glm::vec2& v, float lo, float hi) { return glm::clamp(v, lo, hi); },
        [](const glm::vec3& v, float lo, float hi) { return glm::clamp(v, lo, hi); },
        [](const glm::vec4& v, float lo, float hi) { return glm::clamp(v, lo, hi); }
    );
    glmTable["radians"] = sol::overload(
        [](float degrees) { return glm::radians(degrees); },
        [](const glm::vec3& degrees) { return glm::radians(degrees); }
    );
    glmTable["degrees"] = sol::overload(
        [](float radians) { return glm::degrees(radians); },
        [](const glm::vec3& radians) { return glm::degrees(radians); }
    );

    // quaternions
    glmTable["angleAxis"] = [](float angleRadians, const glm::vec3& axis) { return glm::angleAxis(angleRadians, axis); };
    glmTable["eulerAngles"] = [](const glm::quat& q) { return glm::eulerAngles(q); };
    glmTable["slerp"] = [](const glm::quat& a, const glm::quat& b, float t) { return glm::slerp(a, b, t); };
    glmTable["inverse"] = sol::overload(
        [](const glm::quat& q) { return glm::inverse(q); },
        [](const glm::mat4& m) { return glm::inverse(m); }
    );
    glmTable["conjugate"] = [](const glm::quat& q) { return glm::conjugate(q); };
    // the bundled glm predates glm::quatLookAt, same right handed construction
    glmTable["quatLookAt"] = [](const glm::vec3& direction, const glm::vec3& up) {
        glm::mat3 basis;
        basis[2] = -glm::normalize(direction);
        basis[0] = glm::normalize(glm::cross(up, basis[2]));
        basis[1] = glm::cross(basis[2], basis[0]);
        return glm::quat_cast(basis);
    };
    glmTable["toMat4"] = [](const glm::quat& q) { return glm::toMat4(q); };
    glmTable["toQuat"] = [](const glm::mat4& m) { return glm::quat_cast(m); };

    // matrices
    glmTable["translate"] = [](const glm::mat4& m, const glm::vec3& v) { return glm::translate(m, v); };
    glmTable["rotate"] = sol::overload(
        [](const glm::mat4& m, float angleRadians, const glm::vec3& axis) { return glm::rotate(m, angleRadians, axis); },
        [](const glm::vec3& v, const glm::quat& q) { return q * v; }
    );
    glmTable["scale"] = [](const glm::mat4& m, const glm::vec3& v) { return glm::scale(m, v); };
    glmTable["transpose"] = [](const glm::mat4& m) { return glm::transpose(m); };
    glmTable["lookAt"] = [](const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) { return glm::lookAt(eye, center, up); };
    glmTable["perspective"] = [](float fovRadians, float aspect, float zNear, float zFar) { return glm::perspective(fovRadians, aspect, zNear, zFar); };
    glmTable["ortho"] = [](float left, float right, float bottom, float top, float zNear, float zFar) {
        return glm::ortho(left, right, bottom, top, zNear, zFar);
    };

    // translation * rotation * scale, same order as TransformComponent::updateTransform
    glmTable["trs"] = sol::overload(
        [](const glm::vec3& t, const glm::quat& r, const glm::vec3& s) {
            return glm::translate(glm::mat4(1.0f), t) * glm::toMat4(r) * glm::scale(glm::mat4(1.0f), s);
        },
        [](const glm::vec3& t, const glm::vec3& eulerRadians, const glm::vec3& s) {
            return glm::translate(glm::mat4(1.0f), t) * glm::toMat4(glm::quat(eulerRadians)) * glm::scale(glm::mat4(1.0f), s);
        }
    );

    glmTable["pi"] = glm::pi<float>();
}
