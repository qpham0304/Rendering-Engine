#include "EventToLuaBindings.h"
#include "core/events/EventManager.h"
#include "core/events/EventSubscription.h"
#include "logging/Logger.h"
#include <memory>

void EventToLuaBindings::exportEvent(sol::state &luaState, Logger* logger)
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
        "GuiFocusedEvent", EventType::GuiFocusedEvent,
        "CameraUpdateEvent", EventType::CameraUpdateEvent,
        "ContactBegin", EventType::ContactBegin,
        "ContactEnd", EventType::ContactEnd,
        "ContactHit", EventType::ContactHit
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
    _registerEvent<ContactBeginEvent>(luaState);
    _registerEvent<ContactEndEvent>(luaState);
    _registerEvent<ContactHitEvent>(luaState);

    EventManager& eventManager = EventManager::getInstance();

    luaState["publish"] = [](Event& event) {
        EventManager& eventManager = EventManager::getInstance();
        eventManager.publish(event);
    };

    luaState.new_usertype<EventSubscription>("EventSubscription",
        sol::no_constructor,
        "unsubscribe", &EventSubscription::reset,
        "isActive", &EventSubscription::isActive
    );

    // a callback usually captures self while self holds the handle, if C++ kept a strong ref to the callback
    // that cycle would never be collected, so the callback lives in Lua tables that only the handle keeps alive
    sol::table handles = luaState.create_table();      // subscription id -> handle (weak values)
    sol::table callbacks = luaState.create_table();    // handle -> callback (weak keys)
    handles[sol::metatable_key] = luaState.create_table_with("__mode", "v");
    callbacks[sol::metatable_key] = luaState.create_table_with("__mode", "k");

    luaState["subscribe"] = [logger, handles, callbacks](sol::this_state state, EventType eventType, sol::protected_function luaCallback) {
        auto subscriptionID = std::make_shared<uint32_t>(0);

        EventSubscription subscription = EventManager::getInstance().subscribe(eventType, [logger, handles, callbacks, subscriptionID](Event& event) {
            sol::object handle = handles[*subscriptionID];
            if (!handle.valid()) {
                return;     // collected
            }

            sol::protected_function callback = callbacks[handle];
            auto result = callback(event);
            if (!result.valid()) {
                sol::error err = result;
                logger->error("Lua event callback error: {}", err.what());
            }
        });
        *subscriptionID = subscription.getID();

        sol::object handle = sol::make_object(state, std::move(subscription));
        sol::table handleTable = handles;       // sol setters are non-const, the captures are
        sol::table callbackTable = callbacks;
        handleTable[*subscriptionID] = handle;
        callbackTable[handle] = luaCallback;
        return handle;
    };

}
