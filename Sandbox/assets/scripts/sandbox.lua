-- Player.lua
Player = {}
Player.__index = Player

-- Constructor (Using a dot '.' means only 'entityWrapper' is passed)
function Player:new(entityWrapper)
    local instance = {
        entity = entityWrapper
    }
    return setmetatable(instance, Player)
end

-- Lifecycle Loop (CRITICAL: Using a colon ':' injects 'self' automatically!)
function Player:onUpdate(deltaTime)
    -- Now 'self' points to the specific instance table, and self.entity works!
    local nameComp = self.entity:getName()
    print("Executing frame logic for entity: " .. nameComp.name)
end