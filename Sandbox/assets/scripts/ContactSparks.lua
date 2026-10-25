-- every hit bursts particles from the contact point off the surface, more for faster impacts
-- uses the entity's ParticleEmitter if it has one, otherwise adds a spark emitter that only emits on request
-- each hit queues its own burst, so several hits in the same frame each spark at their own point

ContactSparks = {}
ContactSparks.__index = ContactSparks

function ContactSparks.new(entityWrapper)
    local instance = {
        entity = entityWrapper,
        hitSubscription = nil,
        beginSubscription = nil,
        endSubscription = nil,

        sparksPerSpeed = 8,   -- particles per m/s of approach speed
        minSparks = 10,
        maxSparks = 200,
        touching = 0,         -- shapes currently in contact
    }
    return setmetatable(instance, ContactSparks)
end

function ContactSparks:onInit()
    local collider = self.entity:getComponent("ColliderComponent")
    if collider == nil or collider.shapeID == 0 then
        log_warn("ContactSparks: no physics body, add a ColliderComponent")
        return
    end

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
end

function ContactSparks:onDestroy()
end
