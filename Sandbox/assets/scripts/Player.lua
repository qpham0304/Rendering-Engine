-- physics test player, works while the engine is playing (bodies are kinematic in the editor)
--
--   A / D or LEFT / RIGHT   run (sets velocity, keeps the vertical part so gravity still works)
--   W / S                   run along z (3D mode only)
--   SPACE                   jump (impulse scaled by mass, only when not already moving vertically)
--   F (hold)                upward force every fixed step, stronger than gravity so it lifts off
--   I / J / K / L           one shot impulses up / left / down / right, the old test keys
--   T                       spin around z with angular velocity
--   R                       cycle rotation locks: all locked (upright) -> only z free (rolls in 2D) -> all free
--   M                       toggle 2D mode (locks z movement) / 3D mode
--   P                       print mass, velocity and locks

Player = {}
Player.__index = Player

-- which rotation axes are locked, cycled with R
Player.rotationModes = {
    { name = "all locked", x = true,  y = true,  z = true  },
    { name = "z free",     x = true,  y = true,  z = false },
    { name = "all free",   x = false, y = false, z = false },
}

function Player.new(entityWrapper)
    local instance = {
        entity = entityWrapper,
        body = 0,                -- collider shapeID, every physics.* call takes it
        keySubscription = nil,   -- ends when this instance is collected (reload, entity removed)

        mode2D = true,
        rotationMode = 1,        -- index into Player.rotationModes

        runSpeed = 5.0,          -- m/s
        acceleration = 30.0,     -- how fast velocity reaches runSpeed, m/s^2
        jumpSpeed = 6.0,         -- m/s added upward
        hitSpeed = 8.0,          -- m/s for the I/J/K/L impulses
        liftAcceleration = 15.0, -- m/s^2, gravity is 9.81
    }
    return setmetatable(instance, Player)
end

function Player:onInit()
    local collider = self.entity:getComponent("ColliderComponent")
    if collider == nil or collider.shapeID == 0 then
        log_warn("Player: no physics body, add a ColliderComponent")
        return
    end
    self.body = collider.shapeID
    self:applyLocks()

    -- one shot actions go through events so a press is never missed between fixed steps
    self.keySubscription = subscribe(EventType.KeyPressed, function(event)
        self:onKeyPressed(as_KeyPressedEvent(event).keyCode)
    end)
end

function Player:onDestroy()
end

-- movement runs at the fixed physics rate, read held keys here
function Player:onFixedUpdate(dt)
    if self.body == 0 then
        return
    end

    local input = vec3(0)
    if isKeyPressed(KeyCodes.KEY_A) or isKeyPressed(KeyCodes.KEY_LEFT) then input.x = input.x - 1 end
    if isKeyPressed(KeyCodes.KEY_D) or isKeyPressed(KeyCodes.KEY_RIGHT) then input.x = input.x + 1 end
    if not self.mode2D then
        if isKeyPressed(KeyCodes.KEY_W) then input.z = input.z - 1 end
        if isKeyPressed(KeyCodes.KEY_S) then input.z = input.z + 1 end
    end
    if glm.length(input) > 1 then
        input = glm.normalize(input)
    end

    -- steer the horizontal velocity toward the target instead of snapping, vertical stays with physics
    local velocity = physics.getLinearVelocity(self.body)
    local target = input * self.runSpeed
    local maxChange = self.acceleration * dt
    velocity.x = velocity.x + glm.clamp(target.x - velocity.x, -maxChange, maxChange)
    velocity.z = velocity.z + glm.clamp(target.z - velocity.z, -maxChange, maxChange)
    physics.setLinearVelocity(self.body, velocity)

    -- a force only lasts one step, so it has to be applied every step while held
    if isKeyPressed(KeyCodes.KEY_F) then
        local mass = physics.getMass(self.body)
        physics.addForce(self.body, vec3(0, mass * self.liftAcceleration, 0))
    end
end

function Player:onKeyPressed(key)
    if self.body == 0 then
        return
    end

    -- impulse = mass * change in velocity, so the same numbers work for any collider size
    local mass = physics.getMass(self.body)

    if key == KeyCodes.KEY_SPACE then
        local velocity = physics.getLinearVelocity(self.body)
        if math.abs(velocity.y) < 0.1 then   -- rough grounded check until raycasts exist
            physics.addImpulse(self.body, vec3(0, mass * self.jumpSpeed, 0))
        end

    elseif key == KeyCodes.KEY_I then physics.addImpulse(self.body, vec3(0, mass * self.hitSpeed, 0))
    elseif key == KeyCodes.KEY_K then physics.addImpulse(self.body, vec3(0, -mass * self.hitSpeed, 0))
    elseif key == KeyCodes.KEY_J then physics.addImpulse(self.body, vec3(-mass * self.hitSpeed, 0, 0))
    elseif key == KeyCodes.KEY_L then physics.addImpulse(self.body, vec3(mass * self.hitSpeed, 0, 0))

    elseif key == KeyCodes.KEY_T then
        if Player.rotationModes[self.rotationMode].z then
            log_info("Player: z rotation is locked, press R until z is free before spinning")
        end
        physics.setAngularVelocity(self.body, vec3(0, 0, glm.radians(360)))

    elseif key == KeyCodes.KEY_R then
        self.rotationMode = self.rotationMode % #Player.rotationModes + 1
        self:applyLocks()
        log_info("Player: rotation " .. Player.rotationModes[self.rotationMode].name)

    elseif key == KeyCodes.KEY_M then
        self.mode2D = not self.mode2D
        self:applyLocks()
        log_info("Player: " .. (self.mode2D and "2D (z locked)" or "3D"))

    elseif key == KeyCodes.KEY_P then
        local locks = physics.getMotionLocks(self.body)
        log_info("Player: mass " .. tostring(math.floor(mass * 1000 + 0.5) / 1000)
            .. "  velocity " .. tostring(physics.getLinearVelocity(self.body))
            .. "  spin " .. tostring(physics.getAngularVelocity(self.body))
            .. "  position locks " .. tostring(locks.linearX) .. " " .. tostring(locks.linearY) .. " " .. tostring(locks.linearZ)
            .. "  rotation locks " .. tostring(locks.angularX) .. " " .. tostring(locks.angularY) .. " " .. tostring(locks.angularZ))
    end
end

function Player:applyLocks()
    local rotation = Player.rotationModes[self.rotationMode]
    physics.lockPosition(self.body, false, false, self.mode2D)   -- 2D keeps the body on its z plane
    physics.lockRotation(self.body, rotation.x, rotation.y, rotation.z)
end
