local a = 5
log_info("value: " .. a)

local activeScene = getActiveScene()
log_info("scene name: ".. activeScene:getName())

local entityName = "TestEntity"
-- activeScene:addEntity(entityName)
-- log_info("addEntity: ".. entityName)

-- activeScene:removeEntity(entityName)
-- log_info("removeEntity: ".. entityName)