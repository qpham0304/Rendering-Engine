Player = {}
Player.__index = Player

function Player.new(entityWrapper)
    local instance = {
        entity = entityWrapper,
        time = 0,
        eventID = 0
    }
    return setmetatable(instance, Player)
end

function Player:onInit()
    log_info("subscribe Event")
    self.eventID = subscribe(EventType.KeyPressed,
        function(event)
            local keyPressedEvent = as_KeyPressedEvent(event)
            local collider = self.entity:getComponent("ColliderComponent")
            if collider == nil then
                log_info("failing to accquire collider")
                return
            end

            local keyPressedEvent = as_KeyPressedEvent(event)
            if (keyPressedEvent.keyCode == KeyCodes.KEY_L) then
                addImpulse(collider.shapeID, vec3(100.0, 0.0, 0.0))
            elseif (keyPressedEvent.keyCode == KeyCodes.KEY_J) then
                addImpulse(collider.shapeID, vec3(-100.0, 0.0, 0.0))
            elseif (keyPressedEvent.keyCode == KeyCodes.KEY_K) then
                addImpulse(collider.shapeID, vec3(0.0, -100, 0.0))
            elseif (keyPressedEvent.keyCode == KeyCodes.KEY_I) then
                addImpulse(collider.shapeID, vec3(0.0, 100, 0.0))
            end
        end
    )
end

function Player:onDestroy()
    log_info("unsubscribe Event")
    unsubscribe(eventType.MouseMoved, self.eventID)
end

function Player:onUpdate(deltaTime)
    local nameComp = self.entity:getComponent("NameComponent")
    local transform = self.entity:getComponent("TransformComponent")
    local collider = self.entity:getComponent("ColliderComponent")
    local step = 0.05

    self.time = self.time + deltaTime

    if transform and transform.translateVec and transform.rotateVec then
        local speed = 4.0   -- How fast it oscillates
        local amplitude = 3.0 -- How far left and right it goes
        
        -- transform.translateVec[1] = math.sin(self.time * speed) * amplitude
        -- transform.rotateVec[3] = transform.rotateVec[3] + (90.0 * deltaTime)
        -- log_info("collider info: ".. collider.shapeID)
        
        
        -- self.entity:setComponent("TransformComponent", transform)
    end

    -- local x, y = getCursorPos()
    -- log_info("Cursor X: " .. tostring(x) .. ", Y: " .. tostring(y))

end