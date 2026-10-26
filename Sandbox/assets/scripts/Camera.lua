Camera = {}
Camera.__index = Camera

function Camera.new(entityWrapper)
    local instance = {
        entity = entityWrapper
        -- isMouseMoved = false
    }
    return setmetatable(instance, Camera)
end

function Camera:processMouse()
    -- local x, y = getCursorPos()
    return true
end

function Camera:processKeyboard()
    local nameComp = self.entity:getComponent("NameComponent")
    local transform = self.entity:getComponent("TransformComponent")
    local cameraComponent = self.entity:getComponent("CameraComponent")
    local isPressing = false
    local step = 0.05

    if transform then
        local position = transform.translateVec
        if isKeyPressed(KeyCodes.KEY_W) then
            position.y = position.y + step
        elseif isKeyPressed(KeyCodes.KEY_S) then
            position.y = position.y - step
        elseif isKeyPressed(KeyCodes.KEY_A) then
            position.x = position.x - step
        elseif isKeyPressed(KeyCodes.KEY_D) then
            position.x = position.x + step
        end
        transform:updateTransform()   -- rebuild the model matrix after editing the fields

        if cameraComponent then
            cameraComponent.view:set(4, 1, -position.x)
            cameraComponent.view:set(4, 2, -position.y)
            cameraComponent.view:set(4, 3, -position.z)
            cameraComponent.view:set(4, 4, 1.0)
        end
    end

    return isPressing
end

function Camera:onInit()
end

function Camera:onUpdate(deltaTime)
    self:processMouse()
    self:processKeyboard()
end