Player = {}
Player.__index = Player

function Player.new(entityWrapper)
    local instance = {
        entity = entityWrapper
    }
    return setmetatable(instance, Player)
end

function Player:onUpdate(deltaTime)
    local nameComp = self.entity:getComponent("NameComponent")
    local transform = self.entity:getComponent("TransformComponent")

    log_info("Executing frame logic for entity: " .. nameComp.name)
    
    if transform and transform.translateVec and transform.rotateVec then
        -- index is 1, 2, 3 equivalent to x, y, z
        transform.translateVec[1] = transform.translateVec[1] + (5.0 * deltaTime)
        transform.rotateVec[3] = transform.rotateVec[3] + (90.0 * deltaTime)
        
        self.entity:setComponent("TransformComponent", transform)
    end
end