#include "SharedFunctionsLua.h"
#include "core/components/MComponent.h"
#include "core/features/ServiceLocator.h"
#include "core/scene/SceneManager.h"
#include "core/events/EventManager.h"
#include "core/entities/Entity.h"
#include "physics/PhysicsManager.h"
#include "window/AppWindow.h"
#include "logging/logger.h"
#include <tuple>

SharedFunctionsLua::SharedFunctionsLua(Logger* logger)
    : m_logger(logger)
{

}

void SharedFunctionsLua::math(sol::state &luaState)
{
    luaState.new_usertype<glm::vec4>("vec4",
        "x", &glm::vec4::x,
        "y", &glm::vec4::y,
        "z", &glm::vec4::z,
        "w", &glm::vec4::w
    );
    luaState["vec4"] = sol::overload(
        []() { return glm::vec4(0.0f); },
        [](float x, float y, float z, float w) { return glm::vec4(x, y, z, w); }
    );

    luaState.new_usertype<glm::vec3>("vec3",
        "x", &glm::vec3::x,
        "y", &glm::vec3::y,
        "z", &glm::vec3::z
    );
    luaState["vec3"] = sol::overload(
        []() { return glm::vec3(0.0f); },
        [](float x, float y, float z) { return glm::vec3(x, y, z); }
    );

    luaState.new_usertype<glm::vec2>("vec2",
        "x", &glm::vec2::x,
        "y", &glm::vec2::y
    );
    luaState["vec2"] = sol::overload(
        []() { return glm::vec2(0.0f); },
        [](float x, float y) { return glm::vec2(x, y); }
    );

    // luaState.new_usertype<glm::vec4>("vec4",
    //     sol::constructors<glm::vec4(), glm::vec4(double, double, double, double)>(),
    //     "x", &glm::vec4::x,
    //     "y", &glm::vec4::y,
    //     "z", &glm::vec4::z,
    //     "w", &glm::vec4::w,
    //     sol::meta_function::index, [](glm::vec4& v, int idx) -> float {
    //         if (idx == 1) return v.x;
    //         if (idx == 2) return v.y;
    //         if (idx == 3) return v.z;
    //         if (idx == 4) return v.w;
    //         throw std::out_of_range("Vector index out of bounds");
    //     },
    //     sol::meta_function::new_index, [](glm::vec4& v, int idx, float val) {
    //         if (idx == 1) v.x = val;
    //         else if (idx == 2) v.y = val;
    //         else if (idx == 3) v.z = val;
    //         else if (idx == 4) v.w = val;
    //         else throw std::out_of_range("Vector index out of bounds");
    //     },
    //     sol::meta_function::addition, [](const glm::vec4& a, const glm::vec4& b) { return a + b; },
    //     sol::meta_function::subtraction, [](const glm::vec4& a, const glm::vec4& b) { return a - b; },
    //     sol::meta_function::multiplication, [](const glm::vec4& a, float scalar) { return a * scalar; },
    //     sol::meta_function::division, [](const glm::vec4& a, sol::object b) {
    //         if(b.is<float>() || b.is<double>()) {
    //             float scalar = b.as<float>();
    //             if((scalar == 0.0f)) {
    //                 throw::std::runtime_error("Error: division by zero");
    //             }
    //             return a / scalar;
    //         } else if(b.is<glm::vec4>()) {
    //             glm::vec4 o = b.as<glm::vec4>();
    //             return glm::vec4(
    //                 o.x != 0.0f ? a.x / o.x : 0.0f,
    //                 o.y != 0.0f ? a.y / o.y : 0.0f,
    //                 o.z != 0.0f ? a.z / o.z : 0.0f,
    //                 o.w != 0.0f ? a.w / o.w : 0.0f
    //             );
    //         }
    //         throw std::runtime_error("Error: invalid argument type for division");
    //     }
    // );

    // luaState.new_usertype<glm::vec3>("vec3",
    //     sol::constructors<glm::vec3(), glm::vec3(double, double, double)>(),
    //     "x", &glm::vec3::x,
    //     "y", &glm::vec3::y,
    //     "z", &glm::vec3::z,
    //     sol::meta_function::index, [](glm::vec3& v, int idx) -> float {
    //         if (idx == 1) return v.x;
    //         if (idx == 2) return v.y;
    //         if (idx == 3) return v.z;
    //         throw std::out_of_range("Vector index out of bounds");
    //     },
    //     sol::meta_function::new_index, [](glm::vec3& v, int idx, float val) {
    //         if (idx == 1) v.x = val;
    //         else if (idx == 2) v.y = val;
    //         else if (idx == 3) v.z = val;
    //         else throw std::out_of_range("Vector index out of bounds");
    //     },
    //     sol::meta_function::addition, [](const glm::vec3& a, const glm::vec3& b) { return a + b; },
    //     sol::meta_function::subtraction, [](const glm::vec3& a, const glm::vec3& b) { return a - b; },
    //     sol::meta_function::multiplication, [](const glm::vec3& a, float scalar) { return a * scalar; },
    //     sol::meta_function::division, [](const glm::vec3& a, sol::object b) {
    //         if(b.is<float>() || b.is<double>()) {
    //             float scalar = b.as<float>();
    //             if((scalar == 0.0f)) {
    //                 throw::std::runtime_error("Error: division by zero");
    //             }
    //             return a / scalar;
    //         } else if(b.is<glm::vec3>()) {
    //             glm::vec3 o = b.as<glm::vec3>();
    //             return glm::vec3(
    //                 o.x != 0.0f ? a.x / o.x : 0.0f,
    //                 o.y != 0.0f ? a.y / o.y : 0.0f,
    //                 o.z != 0.0f ? a.z / o.z : 0.0f
    //             );
    //         }
    //         throw std::runtime_error("Error: invalid argument type for division");
    //     }
    // );

    // luaState.new_usertype<glm::vec2>("vec2",
    //     sol::constructors<glm::vec2(), glm::vec2(double, double)>(),
    //     "x", &glm::vec2::x,
    //     "y", &glm::vec2::y,
    //     sol::meta_function::index, [](glm::vec2& v, int idx) -> float {
    //         if (idx == 1) return v.x;
    //         if (idx == 2) return v.y;
    //         throw std::out_of_range("Vector index out of bounds");
    //     },
    //     sol::meta_function::new_index, [](glm::vec2& v, int idx, float val) {
    //         if (idx == 1) v.x = val;
    //         else if (idx == 2) v.y = val;
    //         else throw std::out_of_range("Vector index out of bounds");
    //     },
    //     sol::meta_function::addition, [](const glm::vec2& a, const glm::vec2& b) { return a + b; },
    //     sol::meta_function::subtraction, [](const glm::vec2& a, const glm::vec2& b) { return a - b; },
    //     sol::meta_function::multiplication, [](const glm::vec2& a, float scalar) { return a * scalar; },
    //     sol::meta_function::division, [](const glm::vec2& a, sol::object b) {
    //         if(b.is<float>() || b.is<double>()) {
    //             float scalar = b.as<float>();
    //             if((scalar == 0.0f)) {
    //                 throw::std::runtime_error("Error: division by zero");
    //             }
    //             return a / scalar;
    //         } else if(b.is<glm::vec2>()) {
    //             glm::vec2 o = b.as<glm::vec2>();
    //             return glm::vec2(
    //                 o.x != 0.0f ? a.x / o.x : 0.0f,
    //                 o.y != 0.0f ? a.y / o.y : 0.0f
    //             );
    //         }
    //         throw std::runtime_error("Error: invalid argument type for division");
    //     }
    // );

    
}

void SharedFunctionsLua::input(sol::state &luaState)
{
    luaState["isMousePressed"] = [](MouseCodes mouseCode) { return AppWindow::isMousePressed(mouseCode); };
    luaState["isKeyPressed"] = [](KeyCodes keyCode) { return AppWindow::isKeyPressed(keyCode); };
    luaState["getMouseButton"] = [](MouseCodes mouseCode) { return AppWindow::getMouseButton(mouseCode); };
    luaState["getCursorPos"] = [] () {
        double x, y;
        AppWindow::getCursorPos(&x, &y);
        return std::make_tuple(x, y);
    };
    luaState["enableCursor"] = []() { AppWindow::enableCursor(); };
    luaState["disableCursor"] = []() { AppWindow::disableCursor(); };
    luaState["getKey"] = [](KeyCodes keyCode) { return AppWindow::getKey(keyCode); };
    luaState["getTime"] = []() { return AppWindow::getTime(); };

    luaState.new_enum("KeyCodes",
        "KEY_SPACE", KeyCodes::KEY_SPACE,
        "KEY_APOSTROPHE", KeyCodes::KEY_APOSTROPHE,
        "KEY_COMMA", KeyCodes::KEY_COMMA,
        "KEY_MINUS", KeyCodes::KEY_MINUS,
        "KEY_PERIOD", KeyCodes::KEY_PERIOD,
        "KEY_SLASH", KeyCodes::KEY_SLASH,
        "KEY_0", KeyCodes::KEY_0,
        "KEY_1", KeyCodes::KEY_1,
        "KEY_2", KeyCodes::KEY_2,
        "KEY_3", KeyCodes::KEY_3,
        "KEY_4", KeyCodes::KEY_4,
        "KEY_5", KeyCodes::KEY_5,
        "KEY_6", KeyCodes::KEY_6,
        "KEY_7", KeyCodes::KEY_7,
        "KEY_8", KeyCodes::KEY_8,
        "KEY_9", KeyCodes::KEY_9,
        "KEY_SEMICOLON", KeyCodes::KEY_SEMICOLON,
        "KEY_EQUAL", KeyCodes::KEY_EQUAL,
        "KEY_A", KeyCodes::KEY_A,
        "KEY_B", KeyCodes::KEY_B,
        "KEY_C", KeyCodes::KEY_C,
        "KEY_D", KeyCodes::KEY_D,
        "KEY_E", KeyCodes::KEY_E,
        "KEY_F", KeyCodes::KEY_F,
        "KEY_G", KeyCodes::KEY_G,
        "KEY_H", KeyCodes::KEY_H,
        "KEY_I", KeyCodes::KEY_I,
        "KEY_J", KeyCodes::KEY_J,
        "KEY_K", KeyCodes::KEY_K,
        "KEY_L", KeyCodes::KEY_L,
        "KEY_M", KeyCodes::KEY_M,
        "KEY_N", KeyCodes::KEY_N,
        "KEY_O", KeyCodes::KEY_O,
        "KEY_P", KeyCodes::KEY_P,
        "KEY_Q", KeyCodes::KEY_Q,
        "KEY_R", KeyCodes::KEY_R,
        "KEY_S", KeyCodes::KEY_S,
        "KEY_T", KeyCodes::KEY_T,
        "KEY_U", KeyCodes::KEY_U,
        "KEY_V", KeyCodes::KEY_V,
        "KEY_W", KeyCodes::KEY_W,
        "KEY_X", KeyCodes::KEY_X,
        "KEY_Y", KeyCodes::KEY_Y,
        "KEY_Z", KeyCodes::KEY_Z,
        "KEY_LEFT_BRACKET", KeyCodes::KEY_LEFT_BRACKET,
        "KEY_BACKSLASH", KeyCodes::KEY_BACKSLASH,
        "KEY_RIGHT_BRACKET", KeyCodes::KEY_RIGHT_BRACKET,
        "KEY_GRAVE_ACCENT", KeyCodes::KEY_GRAVE_ACCENT,
        "KEY_WORLD_1", KeyCodes::KEY_WORLD_1,
        "KEY_WORLD_2", KeyCodes::KEY_WORLD_2,

        /* Function keys */
        "KEY_ESCAPE", KeyCodes::KEY_ESCAPE,
        "KEY_ENTER", KeyCodes::KEY_ENTER,
        "KEY_TAB", KeyCodes::KEY_TAB,
        "KEY_BACKSPACE", KeyCodes::KEY_BACKSPACE,
        "KEY_INSERT", KeyCodes::KEY_INSERT,
        "KEY_DELETE", KeyCodes::KEY_DELETE,
        "KEY_RIGHT", KeyCodes::KEY_RIGHT,
        "KEY_LEFT", KeyCodes::KEY_LEFT,
        "KEY_DOWN", KeyCodes::KEY_DOWN,
        "KEY_UP", KeyCodes::KEY_UP,
        "KEY_PAGE_UP", KeyCodes::KEY_PAGE_UP,
        "KEY_PAGE_DOWN", KeyCodes::KEY_PAGE_DOWN,
        "KEY_HOME", KeyCodes::KEY_HOME,
        "KEY_END", KeyCodes::KEY_END,
        "KEY_CAPS_LOCK", KeyCodes::KEY_CAPS_LOCK,
        "KEY_SCROLL_LOCK", KeyCodes::KEY_SCROLL_LOCK,
        "KEY_NUM_LOCK", KeyCodes::KEY_NUM_LOCK,
        "KEY_PRINT_SCREEN", KeyCodes::KEY_PRINT_SCREEN,
        "KEY_PAUSE", KeyCodes::KEY_PAUSE,
        "KEY_F1", KeyCodes::KEY_F1,
        "KEY_F2", KeyCodes::KEY_F2,
        "KEY_F3", KeyCodes::KEY_F3,
        "KEY_F4", KeyCodes::KEY_F4,
        "KEY_F5", KeyCodes::KEY_F5,
        "KEY_F6", KeyCodes::KEY_F6,
        "KEY_F7", KeyCodes::KEY_F7,
        "KEY_F8", KeyCodes::KEY_F8,
        "KEY_F9", KeyCodes::KEY_F9,
        "KEY_F10", KeyCodes::KEY_F10,
        "KEY_F11", KeyCodes::KEY_F11,
        "KEY_F12", KeyCodes::KEY_F12,
        "KEY_F13", KeyCodes::KEY_F13,
        "KEY_F14", KeyCodes::KEY_F14,
        "KEY_F15", KeyCodes::KEY_F15,
        "KEY_F16", KeyCodes::KEY_F16,
        "KEY_F17", KeyCodes::KEY_F17,
        "KEY_F18", KeyCodes::KEY_F18,
        "KEY_F19", KeyCodes::KEY_F19,
        "KEY_F20", KeyCodes::KEY_F20,
        "KEY_F21", KeyCodes::KEY_F21,
        "KEY_F22", KeyCodes::KEY_F22,
        "KEY_F23", KeyCodes::KEY_F23,
        "KEY_F24", KeyCodes::KEY_F24,
        "KEY_F25", KeyCodes::KEY_F25,
        "KEY_KP_0", KeyCodes::KEY_KP_0,
        "KEY_KP_1", KeyCodes::KEY_KP_1,
        "KEY_KP_2", KeyCodes::KEY_KP_2,
        "KEY_KP_3", KeyCodes::KEY_KP_3,
        "KEY_KP_4", KeyCodes::KEY_KP_4,
        "KEY_KP_5", KeyCodes::KEY_KP_5,
        "KEY_KP_6", KeyCodes::KEY_KP_6,
        "KEY_KP_7", KeyCodes::KEY_KP_7,
        "KEY_KP_8", KeyCodes::KEY_KP_8,
        "KEY_KP_9", KeyCodes::KEY_KP_9,
        "KEY_KP_DECIMAL", KeyCodes::KEY_KP_DECIMAL,
        "KEY_KP_DIVIDE", KeyCodes::KEY_KP_DIVIDE,
        "KEY_KP_MULTIPLY", KeyCodes::KEY_KP_MULTIPLY,
        "KEY_KP_SUBTRACT", KeyCodes::KEY_KP_SUBTRACT,
        "KEY_KP_ADD", KeyCodes::KEY_KP_ADD,
        "KEY_KP_ENTER", KeyCodes::KEY_KP_ENTER,
        "KEY_KP_EQUAL", KeyCodes::KEY_KP_EQUAL,
        "KEY_LEFT_SHIFT", KeyCodes::KEY_LEFT_SHIFT,
        "KEY_LEFT_CONTROL", KeyCodes::KEY_LEFT_CONTROL,
        "KEY_LEFT_ALT", KeyCodes::KEY_LEFT_ALT,
        "KEY_LEFT_SUPER", KeyCodes::KEY_LEFT_SUPER,
        "KEY_RIGHT_SHIFT", KeyCodes::KEY_RIGHT_SHIFT,
        "KEY_RIGHT_CONTROL", KeyCodes::KEY_RIGHT_CONTROL,
        "KEY_RIGHT_ALT", KeyCodes::KEY_RIGHT_ALT,
        "KEY_RIGHT_SUPER", KeyCodes::KEY_RIGHT_SUPER,
        "KEY_MENU", KeyCodes::KEY_MENU
    );

    luaState.new_enum("MouseCodes",
        "MOUSE_BUTTON_1", MouseCodes::MOUSE_BUTTON_1,
        "MOUSE_BUTTON_2", MouseCodes::MOUSE_BUTTON_2,
        "MOUSE_BUTTON_3", MouseCodes::MOUSE_BUTTON_3,
        "MOUSE_BUTTON_4", MouseCodes::MOUSE_BUTTON_4,
        "MOUSE_BUTTON_5", MouseCodes::MOUSE_BUTTON_5,
        "MOUSE_BUTTON_6", MouseCodes::MOUSE_BUTTON_6,
        "MOUSE_BUTTON_7", MouseCodes::MOUSE_BUTTON_7,
        "MOUSE_BUTTON_8", MouseCodes::MOUSE_BUTTON_8,
        "MOUSE_BUTTON_LAST", MouseCodes::MOUSE_BUTTON_LAST,
        "MOUSE_BUTTON_LEFT", MouseCodes::MOUSE_BUTTON_LEFT,
        "MOUSE_BUTTON_RIGHT", MouseCodes::MOUSE_BUTTON_RIGHT,
        "MOUSE_BUTTON_MIDDLE", MouseCodes::MOUSE_BUTTON_MIDDLE
    );
}

void SharedFunctionsLua::event(sol::state& luaState )
{
    luaState.new_enum("EventType",
        "None", EventType::None,
        "WindowClose", EventType::WindowClose,
        "WindowResize", EventType::WindowResize,
        "WindowFocus", EventType::WindowFocus,
        "WindowLostFocus", EventType::WindowLostFocus,
        "WindowMoved", EventType::WindowMoved,
        "WindowUpdate", EventType::WindowUpdate,
        "KeyPressed", EventType::KeyPressed,
        "KeyCombined", EventType::KeyCombined,
        "KeyReleased", EventType::KeyReleased,
        "KeyTyped", EventType::KeyTyped,
        "MousePressed", EventType::MousePressed,
        "MouseReleased", EventType::MouseReleased,
        "MouseMoved", EventType::MouseMoved,
        "MouseScrolled", EventType::MouseScrolled,
        "AsyncEvent", EventType::AsyncEvent,
        "ModelLoadEvent", EventType::ModelLoadEvent,
        "AnimationLoadEvent", EventType::AnimationLoadEvent,
        "GuiMessageEvent", EventType::GuiMessageEvent,
        "GuiFocusedEvent", EventType::GuiFocusedEvent
    );

    luaState.new_usertype<Event>("Event",
        "getEventType", &Event::GetEventType,
        "getName", &Event::GetName
    );

    // luaState.new_usertype<KeyPressedEvent>("KeyPressedEvent",
    //     sol::base_classes, sol::bases<Event>(),
    //     "key_code", &KeyPressedEvent::keyCode
    // );

    _registerEvent<MouseMoveEvent>(luaState);
    _registerEvent<MouseScrollEvent>(luaState);
    _registerEvent<KeyPressedEvent>(luaState);
    _registerEvent<KeyCombinedEvent>(luaState);
    // _registerEvent<WindowCloseEvent>(luaState);
    // _registerEvent<WindowResizeEvent>(luaState);
    // _registerEvent<AsyncEvent>(luaState);
    // _registerEvent<ModelLoadAsyncEvent>(luaState);
    // _registerEvent<ModelLoadEvent>(luaState);
    // _registerEvent<AnimationLoadEvent>(luaState);
    // _registerEvent<GuiMessageEvent>(luaState);
    // _registerEvent<GuiFocusEvent>(luaState);
    _registerEvent<CameraUpdateEvent>(luaState);

    EventManager& eventManager = EventManager::getInstance();

    luaState["publish"] = [](Event& event) {
        EventManager& eventManager = EventManager::getInstance();
        eventManager.publish(event);
    };

    luaState["subscribe"] = [](EventType eventType, sol::function luaCallback) {
        EventManager& eventManager = EventManager::getInstance();
        return eventManager.subscribe(eventType, [luaCallback](Event& event) {
            luaCallback(event);
        });
    };

    luaState["unsubscribe"] = [](EventType eventType, uint32_t id) {
        EventManager& eventManager = EventManager::getInstance();
        eventManager.unsubscribe(eventType, id);
    };

}

void SharedFunctionsLua::component(sol::state& luaState )
{
    luaState.new_usertype<NameComponent>("NameComponent",
        "name", &NameComponent::name
    );

    luaState.new_usertype<TransformComponent>("TransformComponent",
        "getModelMatrix", &TransformComponent::getModelMatrix,
        "updateTransform", &TransformComponent::updateTransform,
        "translate",       &TransformComponent::translate,
        "rotate",          &TransformComponent::rotate,
        "scale",            &TransformComponent::scale,
        "translateVec",     &TransformComponent::translateVec,
        "rotateVec",        &TransformComponent::rotateVec,
        "scaleVec",         &TransformComponent::scaleVec
    );
}

void SharedFunctionsLua::scene(sol::state& luaState )
{
    luaState["getActiveScene"] = [] () -> Scene* {
        SceneManager& sceneManager = SceneManager::getInstance();
        return sceneManager.getActiveScene();
    };

    luaState.new_usertype<Scene>("Scene",
        "addEntity", &Scene::addEntity,
        "removeEntity", sol::overload(
            sol::resolve<bool(const std::string&)>(&Scene::removeEntity),
            sol::resolve<bool(const uint32_t&)>(&Scene::removeEntity)
        ),
        "getName", &Scene::getName
    );

}

void SharedFunctionsLua::logging(sol::state& luaState )
{
    Logger& clientLogger = ServiceLocator::GetService<Logger>("Client_LoggerSPD");

    luaState["log_error"] = [&](const std::string& msg) { clientLogger.error(msg); };
    luaState["log_info"] = [&](const std::string& msg) { clientLogger.info(msg); };
    luaState["log_warn"] = [&](const std::string& msg) { clientLogger.warn(msg); };
    luaState["log_critical"] = [&](const std::string& msg) { clientLogger.critical(msg); };
}

void SharedFunctionsLua::physics(sol::state &luaState)
{
    PhysicsManager* physicsManager = &ServiceLocator::GetService<PhysicsManager>("PhysicsManager");

    luaState["addForce"] = [&] (uint32_t id, glm::vec3 force) { 
        physicsManager->addForce(id, force); 
    };
    luaState["rayCastClosest"] = [&] (glm::vec3 position) -> uint32_t { 
        return physicsManager->rayCastClosest(position); 
    };    
    luaState["addImpulse"] = [physicsManager] (uint32_t id, glm::vec3 impulse) { 
        physicsManager->addImpulse(id, impulse); 
    };
}
