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
    -- log_info("Cursor X: " .. tostring(x) .. ", Y: " .. tostring(y))
    return true
end

function Camera:processKeyboard()
    local nameComp = self.entity:getComponent("NameComponent")
    local transform = self.entity:getComponent("TransformComponent")
    local cameraComponent = self.entity:getComponent("CameraComponent")
    local isPressing = false
    local step = 0.05

    if transform and transform.translateVec then
        if isKeyPressed(KeyCodes.KEY_W) then
            transform.translateVec[2] = transform.translateVec[2] + step
        elseif isKeyPressed(KeyCodes.KEY_S) then
            transform.translateVec[2] = transform.translateVec[2] - step
        elseif isKeyPressed(KeyCodes.KEY_A) then
            transform.translateVec[1] = transform.translateVec[1] - step
        elseif isKeyPressed(KeyCodes.KEY_D) then
            transform.translateVec[1] = transform.translateVec[1] + step
        end
    end

    if cameraComponent and cameraComponent.view then
        cameraComponent.view[4][1] = -transform.translateVec[1]
        cameraComponent.view[4][2] = -transform.translateVec[2]
        cameraComponent.view[4][3] = -transform.translateVec[3]
        cameraComponent.view[4][4] = 1.0
    end

    self.entity:setComponent("TransformComponent", transform)
    self.entity:setComponent("CameraComponent", cameraComponent)

    return isPressing
end

function Camera:onInit()
    -- local eventID = subscribe(EventType.MouseMoved,
    --     function(event)
    --         local mouseMovedEvent = as_MouseMoveEvent(event)
    --         if mouseMovedEvent ~= nil then
    --             log_info("move: " .. mouseMovedEvent.m_x .. ", " .. mouseMovedEvent.m_y)
    --         else
    --             log_info("failing")
    --         end
    --     end
    -- )
end

function Camera:onUpdate(deltaTime)
    self:processMouse()
    self:processKeyboard()
end