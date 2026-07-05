Camera = {}
Camera.__index = Camera

function Camera.new(entityWrapper)
    local instance = {
        entity = entityWrapper
    }
    return setmetatable(instance, Camera)
end

function Camera:onUpdate(deltaTime)
    local nameComp = self.entity:getComponent("NameComponent")
    -- log_info("-----Executing Camera logic for entity: " .. nameComp.name)
    -- log_info("Executing Camera Logic")
end