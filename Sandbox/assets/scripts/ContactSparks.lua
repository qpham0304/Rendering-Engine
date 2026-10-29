-- example script for contact spark behavior
-- every hit bursts particles from the contact point off the surface, more for faster impacts
-- uses the entity's ParticleEmitter if it has one, otherwise adds a spark emitter that only emits on request
-- each hit queues its own burst, so several hits in the same frame each spark at their own point
--
-- gliding: Begin/End keep a touching flag, while it is set and the body moves sideways sparks stream from its feet
-- (center minus footOffset), so this assumes flat ground. they come out of a second emitter,
-- the first child entity with a ParticleEmitter (set it up burst only: emitRate 0, burstInterval 0, its own color)
-- an entity has one emitter and its color applies to all its particles, so a different look needs another emitter
ContactSparks = {}
ContactSparks.__index = ContactSparks

function ContactSparks.new(entityWrapper)
    local instance = {
        entity = entityWrapper,
        body = 0,
        glideEmitter = nil,   -- child entity
        hitSubscription = nil,
        beginSubscription = nil,
        endSubscription = nil,
        keySubscription = nil,

        sparksPerSpeed = 8,   -- impact particles per m/s of approach speed
        minSparks = 10,
        maxSparks = 200,
        touching = 0,         -- shapes currently in contact

        glideMinSpeed = 0.5,  -- horizontal m/s before it sparks
        footOffset = 0.5,     -- body center to the ground it slides on, half the box height
        glideSparksPerMeter = 120,
        glideAccumulator = 0, -- fractional sparks carried to the next step

        glideTest = true,
        launchVelocity = vec3(6, 0, 0),
        launched = false,
    }
    return setmetatable(instance, ContactSparks)
end

function ContactSparks:onInit()
    local collider = self.entity:getComponent("ColliderComponent")
    if collider == nil or collider.shapeID == 0 then
        log_warn("ContactSparks: no physics body, add a ColliderComponent")
        return
    end
    self.body = collider.shapeID

    if self.entity:getComponent("ParticleEmitter") == nil then
        self.entity:addComponent("ParticleEmitter", {
            emitRate = 0,
            burstInterval = 0,
            maxParticles = 2000,
            spawnShape = 0,             -- ParticleSpawnShape.Point
            spreadAngle = 70,
            speedMin = 2,
            speedMax = 6,
            lifetimeMin = 0.3,
            lifetimeMax = 0.8,
            gravityScale = 1,
            drag = 1,
            sizeMin = 0.03,
            sizeMax = 0.08,
            sizeEndScale = 0.2,
            colorStart = vec4(1.0, 0.8, 0.3, 1.0),
            colorEnd = vec4(1.0, 0.2, 0.0, 0.0),
            renderMode = 2,             -- ParticleRenderMode.VelocityStretched
        })
    end

    for _, child in ipairs(self.entity:getChildren()) do
        if child:getComponent("ParticleEmitter") ~= nil then
            self.glideEmitter = child
            break
        end
    end
    if self.glideEmitter == nil then
        log_warn("ContactSparks: no child with a ParticleEmitter, glide sparks are off")
    end

    local myID = self.entity:id()

    self.hitSubscription = subscribe(EventType.ContactHit, function(event)
        local hit = as_ContactHitEvent(event)
        if hit.entityA ~= myID and hit.entityB ~= myID then
            return
        end

        local count = math.floor(hit.approachSpeed * self.sparksPerSpeed)
        count = math.max(self.minSparks, math.min(count, self.maxSparks))

        -- the normal points from A to B, flip it so the sparks fly away from the other shape
        local direction = hit.entityA == myID and -hit.normal or hit.normal
        self.entity:getComponent("ParticleEmitter"):burstAt(hit.point, direction, count)
        log_info("ContactSparks: hit at " .. tostring(hit.point) .. " speed " .. tostring(math.floor(hit.approachSpeed * 100 + 0.5) / 100) .. ", " .. count .. " sparks")
    end)

    self.beginSubscription = subscribe(EventType.ContactBegin, function(event)
        local contact = as_ContactBeginEvent(event)
        if contact.entityA == myID or contact.entityB == myID then
            self.touching = self.touching + 1
        end
    end)

    self.endSubscription = subscribe(EventType.ContactEnd, function(event)
        local contact = as_ContactEndEvent(event)
        if contact.entityA == myID or contact.entityB == myID then
            self.touching = math.max(self.touching - 1, 0)
        end
    end)

    self.keySubscription = subscribe(EventType.KeyPressed, function(event)
        if self.glideTest and as_KeyPressedEvent(event).keyCode == KeyCodes.KEY_G then
            self:launch()
        end
    end)
end

function ContactSparks:launch()
    local velocity = physics.getLinearVelocity(self.body)
    physics.setLinearVelocity(self.body, vec3(self.launchVelocity.x, velocity.y, self.launchVelocity.z))
end

function ContactSparks:onFixedUpdate(dt)
    if self.body == 0 then
        return
    end

    -- launched once it is down, a launch in the air loses most of its speed to friction on the landing impact
    if self.glideTest and not self.launched and self.touching > 0 then
        self:launch()
        self.launched = true
    end

    if self.glideEmitter == nil or self.touching == 0 then
        self.glideAccumulator = 0
        return
    end

    -- flat ground: the horizontal part is the slide, the vertical part is falling or bouncing
    local velocity = physics.getLinearVelocity(self.body)
    local slide = vec3(velocity.x, 0, velocity.z)
    local speed = glm.length(slide)
    if speed <= self.glideMinSpeed then
        self.glideAccumulator = 0
        return
    end

    -- spark count follows the distance slid, so it does not depend on the step rate
    self.glideAccumulator = self.glideAccumulator + speed * dt * self.glideSparksPerMeter
    local count = math.floor(self.glideAccumulator)
    self.glideAccumulator = self.glideAccumulator - count
    if count == 0 then
        return
    end

    -- kick up off the ground and back against the motion, like a grinder trail
    local position = self.entity:getComponent("TransformComponent").translateVec
    local feet = vec3(position.x, position.y - self.footOffset, position.z)
    local direction = glm.normalize(vec3(0, 1, 0) - slide / speed)
    self.glideEmitter:getComponent("ParticleEmitter"):burstAt(feet, direction, count)
end

function ContactSparks:onDestroy()
end
