-- ====================================================================
-- FIREBLAST - Space Shooter LED Effect
-- Autonomous AI-controlled space shooter game visualization
-- ====================================================================

effect_name = "Fireblast"
effect_description = "AI space shooter destroying asteroids"

-- Game State
local player = {}
local projectiles = {}
local asteroids = {}
local explosions = {}
local stars = {}

local score = 0
local ships = 3
local shootCooldown = 0
local wave = 1
local lastTime = 0

---------------------------------------------------------
-- Stars Background
---------------------------------------------------------

local function initStars()
    stars = {}
    for i = 1, 30 do
        table.insert(stars, {
            x = math.random(0, WIDTH - 1),
            y = math.random(0, HEIGHT - 1),
            brightness = math.random(40, 100),
            twinklePhase = math.random() * math.pi * 2
        })
    end
end

local function updateStars(dt)
    for _, star in ipairs(stars) do
        star.twinklePhase = star.twinklePhase + dt * 3
    end
end

local function drawStars()
    for _, star in ipairs(stars) do
        local twinkle = math.sin(star.twinklePhase) * 20
        local b = star.brightness + twinkle
        setPixel(star.x, star.y, math.floor(b * 0.8), math.floor(b * 0.8), math.floor(b + 30))
    end
end

---------------------------------------------------------
-- Asteroids
---------------------------------------------------------

local function initAsteroids()
    asteroids = {}
    local rows = 2
    local cols = 6
    local startX = 20
    local startY = 15
    local spacingX = 18
    local spacingY = 12

    for row = 0, rows - 1 do
        for col = 0, cols - 1 do
            table.insert(asteroids, {
                x = startX + col * spacingX,
                y = startY + row * spacingY,
                baseX = startX + col * spacingX,
                baseY = startY + row * spacingY,
                radius = 6,
                alive = true,
                phase = math.random() * math.pi * 2,
                driftSpeed = 0.5 + wave * 0.2
            })
        end
    end
end

local function updateAsteroids(dt)
    for _, ast in ipairs(asteroids) do
        if ast.alive then
            ast.phase = ast.phase + dt * 2
            ast.x = ast.baseX + math.sin(ast.phase) * 2
            ast.baseY = ast.baseY + ast.driftSpeed * dt
            ast.y = ast.baseY

            if ast.y > HEIGHT + 10 then
                ast.baseY = -10
                ast.y = ast.baseY
            end
        end
    end
end

local function drawAsteroid(cx, cy, radius)
    for y = cy - radius, cy + radius do
        for x = cx - radius, cx + radius do
            local dx = x - cx
            local dy = y - cy
            local dist = math.sqrt(dx*dx + dy*dy)
            if dist <= radius then
                local shade = 120 + (radius - dx - dy) * 3
                shade = math.max(80, math.min(180, shade))
                setPixel(math.floor(x), math.floor(y), shade, shade, shade)
            end
        end
    end
end

local function drawAsteroids()
    for _, ast in ipairs(asteroids) do
        if ast.alive then
            drawAsteroid(ast.x, ast.y, ast.radius)
        end
    end
end

---------------------------------------------------------
-- Player Ship
---------------------------------------------------------

local function drawShip(cx, cy)
    -- Main body (blue triangle)
    for y = 0, 7 do
        local halfWidth = math.floor((7 - y) * 0.7)
        for x = -halfWidth, halfWidth do
            local shade = 100 + (7 - y) * 15
            setPixel(math.floor(cx + x), math.floor(cy - 7 + y), 80, 80, shade)
        end
    end

    -- Cockpit (white/light blue)
    setPixel(math.floor(cx), math.floor(cy - 5), 200, 200, 255)
    setPixel(math.floor(cx), math.floor(cy - 4), 180, 180, 240)
    setPixel(math.floor(cx - 1), math.floor(cy - 3), 150, 150, 200)
    setPixel(math.floor(cx + 1), math.floor(cy - 3), 150, 150, 200)

    -- Wings
    setPixel(math.floor(cx - 4), math.floor(cy - 1), 60, 60, 140)
    setPixel(math.floor(cx + 4), math.floor(cy - 1), 60, 60, 140)
    setPixel(math.floor(cx - 5), math.floor(cy), 60, 60, 140)
    setPixel(math.floor(cx + 5), math.floor(cy), 60, 60, 140)

    -- Engine flame
    local flameIntensity = 200 + math.random(0, 55)
    setPixel(math.floor(cx), math.floor(cy + 1), flameIntensity, math.floor(flameIntensity * 0.5), 0)
    setPixel(math.floor(cx), math.floor(cy + 2), math.floor(flameIntensity * 0.8), math.floor(flameIntensity * 0.3), 0)
    if math.random() > 0.6 then
        setPixel(math.floor(cx), math.floor(cy + 3), math.floor(flameIntensity * 0.5), 0, 0)
    end
end

---------------------------------------------------------
-- AI Player Control
---------------------------------------------------------

local function findNearestAsteroid()
    local nearest = nil
    local nearestDist = math.huge

    for _, ast in ipairs(asteroids) do
        if ast.alive then
            local dist = math.abs(ast.x - player.x) + (ast.y * 0.5)
            if dist < nearestDist then
                nearestDist = dist
                nearest = ast
            end
        end
    end

    return nearest
end

local function updateAI(dt, time)
    local target = findNearestAsteroid()

    if target then
        local predictedX = target.x + math.sin(target.phase + 0.5) * 2
        player.targetX = predictedX
        player.targetX = player.targetX + math.sin(time * 4) * 5
    else
        player.targetX = WIDTH / 2 + math.sin(time * 2) * 40
    end

    local diff = player.targetX - player.x
    local moveSpeed = player.speed + 10

    if math.abs(diff) > 2 then
        if diff > 0 then
            player.x = player.x + moveSpeed * dt
        else
            player.x = player.x - moveSpeed * dt
        end
    end

    player.x = math.max(8, math.min(WIDTH - 8, player.x))
end

---------------------------------------------------------
-- Projectiles
---------------------------------------------------------

local function shoot()
    table.insert(projectiles, {
        x = player.x,
        y = player.y - 8,
        speed = 90
    })
end

local function updateProjectiles(dt)
    for i = #projectiles, 1, -1 do
        local p = projectiles[i]
        p.y = p.y - p.speed * dt
        if p.y < 0 then
            table.remove(projectiles, i)
        end
    end
end

local function drawProjectiles()
    for _, p in ipairs(projectiles) do
        setPixel(math.floor(p.x), math.floor(p.y), 255, 50, 200)
        setPixel(math.floor(p.x), math.floor(p.y + 1), 200, 30, 150)
        setPixel(math.floor(p.x), math.floor(p.y + 2), 150, 20, 100)
        setPixel(math.floor(p.x), math.floor(p.y + 3), 100, 10, 60)
    end
end

---------------------------------------------------------
-- Explosions
---------------------------------------------------------

local function createExplosion(x, y)
    table.insert(explosions, {
        x = x,
        y = y,
        timer = 0,
        maxTime = 0.4
    })
end

local function updateExplosions(dt)
    for i = #explosions, 1, -1 do
        local e = explosions[i]
        e.timer = e.timer + dt
        if e.timer >= e.maxTime then
            table.remove(explosions, i)
        end
    end
end

local function drawExplosions()
    for _, e in ipairs(explosions) do
        local progress = e.timer / e.maxTime
        local radius = 4 + progress * 6

        for angle = 0, math.pi * 2, 0.3 do
            local x = e.x + math.cos(angle) * radius
            local y = e.y + math.sin(angle) * radius
            local brightness = math.floor(255 * (1 - progress))
            setPixel(math.floor(x), math.floor(y), brightness, brightness, brightness)
        end

        local coreRadius = radius * 0.5
        for y = e.y - coreRadius, e.y + coreRadius do
            for x = e.x - coreRadius, e.x + coreRadius do
                local dx = x - e.x
                local dy = y - e.y
                if dx*dx + dy*dy <= coreRadius*coreRadius then
                    local intensity = math.floor(255 * (1 - progress))
                    setPixel(math.floor(x), math.floor(y), intensity, math.floor(intensity * 0.6), 0)
                end
            end
        end
    end
end

---------------------------------------------------------
-- Collision Detection
---------------------------------------------------------

local function checkCollisions()
    for pi = #projectiles, 1, -1 do
        local p = projectiles[pi]
        for _, ast in ipairs(asteroids) do
            if ast.alive then
                local dx = p.x - ast.x
                local dy = p.y - ast.y
                if dx*dx + dy*dy < (ast.radius + 2)^2 then
                    ast.alive = false
                    table.remove(projectiles, pi)
                    createExplosion(ast.x, ast.y)
                    score = score + 125
                    break
                end
            end
        end
    end
end

---------------------------------------------------------
-- HUD - Score and Ships
---------------------------------------------------------

local digits = {
    [0] = {{1,1,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1}},
    [1] = {{0,1,0},{1,1,0},{0,1,0},{0,1,0},{1,1,1}},
    [2] = {{1,1,1},{0,0,1},{1,1,1},{1,0,0},{1,1,1}},
    [3] = {{1,1,1},{0,0,1},{1,1,1},{0,0,1},{1,1,1}},
    [4] = {{1,0,1},{1,0,1},{1,1,1},{0,0,1},{0,0,1}},
    [5] = {{1,1,1},{1,0,0},{1,1,1},{0,0,1},{1,1,1}},
    [6] = {{1,1,1},{1,0,0},{1,1,1},{1,0,1},{1,1,1}},
    [7] = {{1,1,1},{0,0,1},{0,0,1},{0,0,1},{0,0,1}},
    [8] = {{1,1,1},{1,0,1},{1,1,1},{1,0,1},{1,1,1}},
    [9] = {{1,1,1},{1,0,1},{1,1,1},{0,0,1},{1,1,1}}
}

local letters = {
    S = {{1,1,1},{1,0,0},{1,1,1},{0,0,1},{1,1,1}},
    C = {{1,1,1},{1,0,0},{1,0,0},{1,0,0},{1,1,1}},
    O = {{1,1,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1}},
    R = {{1,1,0},{1,0,1},{1,1,0},{1,0,1},{1,0,1}},
    E = {{1,1,1},{1,0,0},{1,1,0},{1,0,0},{1,1,1}},
    H = {{1,0,1},{1,0,1},{1,1,1},{1,0,1},{1,0,1}},
    I = {{1,1,1},{0,1,0},{0,1,0},{0,1,0},{1,1,1}},
    P = {{1,1,1},{1,0,1},{1,1,1},{1,0,0},{1,0,0}}
}

local function drawChar(char, startX, startY, r, g, b)
    local pattern = letters[char] or digits[tonumber(char)]
    if not pattern then return end

    for row = 1, 5 do
        for col = 1, 3 do
            if pattern[row][col] == 1 then
                setPixel(startX + col - 1, startY + row - 1, r, g, b)
            end
        end
    end
end

local function drawText(text, startX, startY, r, g, b)
    local x = startX
    for i = 1, #text do
        local char = text:sub(i, i)
        if char == "," then
            setPixel(x, startY + 4, r, g, b)
            x = x + 2
        elseif char == " " then
            x = x + 3
        else
            drawChar(char, x, startY, r, g, b)
            x = x + 5
        end
    end
end

local function drawHUD()
    drawText("SCORE,", 4, 2, 0, 255, 0)
    local scoreStr = string.format("%06d", score)
    drawText(scoreStr, 34, 2, 0, 255, 0)

    drawText("SHIPS", 85, 2, 0, 255, 0)
    drawText(tostring(ships), 115, 2, 0, 255, 0)
end

---------------------------------------------------------
-- Effect API
---------------------------------------------------------

function init(width, height)
    player = {
        x = WIDTH / 2,
        y = HEIGHT - 10,
        speed = 40,
        dir = 1,
        targetX = WIDTH / 2
    }
    projectiles = {}
    asteroids = {}
    explosions = {}
    stars = {}
    score = 0
    ships = 3
    shootCooldown = 0
    wave = 1
    lastTime = 0

    initStars()
    initAsteroids()
end

function reset()
    init(WIDTH, HEIGHT)
end

function update(audio, settings, time)
    local dt = time - lastTime
    if dt <= 0 or dt > 0.1 then dt = 0.016 end
    lastTime = time

    clear()

    -- AI controls the ship
    updateAI(dt, time)

    -- Auto-shooting based on targeting
    shootCooldown = shootCooldown - dt
    local target = findNearestAsteroid()

    if target and shootCooldown <= 0 then
        local dx = math.abs(target.x - player.x)
        if dx < 8 then
            shoot()
            shootCooldown = 0.2
        end
    end

    -- Update game elements
    updateStars(dt)
    updateAsteroids(dt)
    updateProjectiles(dt)
    updateExplosions(dt)
    checkCollisions()

    -- Check if all asteroids destroyed - spawn new wave
    local allDestroyed = true
    for _, ast in ipairs(asteroids) do
        if ast.alive then
            allDestroyed = false
            break
        end
    end
    if allDestroyed then
        wave = wave + 1
        initAsteroids()
    end

    -- Draw everything
    drawStars()
    drawAsteroids()
    drawProjectiles()
    drawExplosions()
    drawShip(player.x, player.y)
    drawHUD()
end
