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
    local camera = self.entity:getComponent("CameraComponent")
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

    if camera and camera.view then
        camera.view[4][1] = -transform.translateVec[1]
        camera.view[4][2] = -transform.translateVec[2]
        camera.view[4][3] = -transform.translateVec[3]
        camera.view[4][4] = 1.0
    end

    camera.orientation[1] = -10.0
    camera.orientation[2] = -10.0
    camera.orientation[3] = -10.0

    self.entity:setComponent("TransformComponent", transform)
    self.entity:setComponent("CameraComponent", camera)

    return isPressing
end

function Camera:onUpdate(deltaTime)
    self:processMouse()
    self:processKeyboard()
end