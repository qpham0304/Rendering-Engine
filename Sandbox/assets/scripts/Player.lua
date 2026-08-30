Player = {}
Player.__index = Player

function Player.new(entityWrapper)
    local instance = {
        entity = entityWrapper
    }
    return setmetatable(instance, Player)
end

local eventID = subscribe(EventType.KeyPressed,
    function(e)
        if isKeyPressed(KeyCodes.KEY_P) then
            log_info("key pressed: P")
        else    
            log_info("key pressed")
        end
    end
)

function Player:onUpdate(deltaTime)
    local nameComp = self.entity:getComponent("NameComponent")
    local transform = self.entity:getComponent("TransformComponent")
    local step = 0.05

    if transform and transform.translateVec and transform.rotateVec then
        -- index is 1, 2, 3 equivalent to x, y, z
        -- transform.translateVec[1] = transform.translateVec[1] + (5.0 * deltaTime)
        transform.rotateVec[3] = transform.rotateVec[3] + (90.0 * deltaTime)
        
        if isKeyPressed(KeyCodes.KEY_J) then
            transform.translateVec[1] = transform.translateVec[1] - step
            log_info("KEY PRESSED KEY_J")
        elseif isKeyPressed(KeyCodes.KEY_K) then
            transform.translateVec[1] = transform.translateVec[1] + step
            -- log_info("KEY PRESSED KEY_K")
            log_info("KEY PRESSED: ".. eventID)
        end
        
        self.entity:setComponent("TransformComponent", transform)
    end

    -- local x, y = getCursorPos()
    -- log_info("Cursor X: " .. tostring(x) .. ", Y: " .. tostring(y))

end