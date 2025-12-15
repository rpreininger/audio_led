---------------------------------------------------------
-- Whac-A-Tourist LED Effect - 128x64
-- Self-running demo with AI player
-- Hit the tourists with the Spanish mallet!
-- Now with real sprites!
---------------------------------------------------------

effect_name = "Whac-A-Tourist"
effect_description = "Whac-A-Mole style game with AI player"

---------------------------------------------------------
-- Load Sprites
---------------------------------------------------------

-- Load sprite data from external file
local sprites_loaded = false
local function loadSprites()
    if sprites_loaded then return end

    -- Try to load the sprites file
    local scriptDir = ""
    if love then
        scriptDir = love.filesystem.getSource() .. "/../scripts/"
    else
        -- For C++ runtime, adjust path as needed
        scriptDir = "effects/scripts/"
    end

    local spritePath = scriptDir .. "sprites/whacatourist_sprites.lua"
    local f = io.open(spritePath, "r")
    if f then
        local content = f:read("*all")
        f:close()
        local chunk = load(content)
        if chunk then
            chunk()
            sprites_loaded = true
        end
    end
end

---------------------------------------------------------
-- Sprite Drawing Functions
---------------------------------------------------------

-- Draw sprite with downscaling (scale < 1)
local function drawSpriteDownscaled(sprite, palette, x, y, scale, skip_zero, clipY)
    if not sprite or not palette then return end

    local srcHeight = #sprite
    local srcWidth = sprite[1] and #sprite[1] or 0

    local dstWidth = math.floor(srcWidth * scale)
    local dstHeight = math.floor(srcHeight * scale)

    for dy = 0, dstHeight - 1 do
        local py = math.floor(y + dy)
        -- Apply clipping if specified
        if (not clipY or py < clipY) and py >= 0 and py < HEIGHT then
            for dx = 0, dstWidth - 1 do
                local px = math.floor(x + dx)
                if px >= 0 and px < WIDTH then
                    -- Sample from source
                    local srcX = math.floor(dx / scale) + 1
                    local srcY = math.floor(dy / scale) + 1

                    if srcY <= srcHeight and srcX <= srcWidth then
                        local color_idx = sprite[srcY][srcX]
                        if not skip_zero or color_idx ~= 0 then
                            local color = palette[color_idx + 1]
                            if color then
                                setPixel(px, py, color[1], color[2], color[3])
                            end
                        end
                    end
                end
            end
        end
    end
end

-- Draw sprite with tint (for hit flash effect)
local function drawSpriteTinted(sprite, palette, x, y, scale, skip_zero, clipY, tintR, tintG, tintB)
    if not sprite or not palette then return end

    local srcHeight = #sprite
    local srcWidth = sprite[1] and #sprite[1] or 0

    local dstWidth = math.floor(srcWidth * scale)
    local dstHeight = math.floor(srcHeight * scale)

    for dy = 0, dstHeight - 1 do
        local py = math.floor(y + dy)
        if (not clipY or py < clipY) and py >= 0 and py < HEIGHT then
            for dx = 0, dstWidth - 1 do
                local px = math.floor(x + dx)
                if px >= 0 and px < WIDTH then
                    local srcX = math.floor(dx / scale) + 1
                    local srcY = math.floor(dy / scale) + 1

                    if srcY <= srcHeight and srcX <= srcWidth then
                        local color_idx = sprite[srcY][srcX]
                        if not skip_zero or color_idx ~= 0 then
                            local color = palette[color_idx + 1]
                            if color then
                                -- Mix with tint
                                local r = math.floor((color[1] + tintR) / 2)
                                local g = math.floor((color[2] + tintG) / 2)
                                local b = math.floor((color[3] + tintB) / 2)
                                setPixel(px, py, r, g, b)
                            end
                        end
                    end
                end
            end
        end
    end
end

---------------------------------------------------------
-- Colors (for background)
---------------------------------------------------------

local BEACH_SAND = {220, 190, 130}
local BEACH_DARK = {180, 150, 100}
local WATER_LIGHT = {80, 180, 220}
local WATER_DARK = {40, 120, 180}
local PALM_GREEN = {30, 140, 50}
local PALM_TRUNK = {100, 70, 40}
local HOLE_DARK = {60, 40, 30}
local HOLE_LIGHT = {100, 70, 50}
local WHITE = {255, 255, 255}
local YELLOW = {255, 220, 50}
local GREEN = {50, 200, 50}
local RED = {255, 50, 50}
local CYAN = {0, 220, 255}

---------------------------------------------------------
-- Game State
---------------------------------------------------------

local gameState = "playing"
local score = 0
local timeLeft = 60
local combo = 0
local comboTimer = 0
local gameTime = 0

-- Mallet (AI controlled)
local mallet = {
    x = 64,
    y = 32,
    frame = 1,
    hitTimer = 0,
    hitDuration = 0.15
}

-- AI state
local ai = {
    targetHole = nil,
    reactionDelay = 0,
    missChance = 0.12,
    moveSpeed = 100,
    waitTimer = 0,
    state = "idle"
}

-- Holes where tourists pop up (3x2 grid)
local holes = {}
local holePositions = {
    {x = 20, y = 40},
    {x = 64, y = 40},
    {x = 108, y = 40},
    {x = 20, y = 56},
    {x = 64, y = 56},
    {x = 108, y = 56}
}

-- Spawn settings
local spawnTimer = 0
local spawnInterval = 1.5
local minSpawnInterval = 0.4
local difficultyTimer = 0

-- Particles
local particles = {}

-- Restart timer
local restartTimer = 0

-- Sprite scale factors
local TOURIST_SCALE = 1.0   -- 32x32 full size
local MALLET_SCALE = 1.0    -- 48x48 full size

---------------------------------------------------------
-- Helper Functions
---------------------------------------------------------

local function spawnParticles(x, y, r, g, b, count)
    for i = 1, count do
        table.insert(particles, {
            x = x,
            y = y,
            vx = (math.random() - 0.5) * 60,
            vy = (math.random() - 0.5) * 60 - 20,
            life = 0.4 + math.random() * 0.3,
            r = r,
            g = g,
            b = b
        })
    end
end

local function updateParticles(dt)
    for i = #particles, 1, -1 do
        local p = particles[i]
        p.x = p.x + p.vx * dt
        p.y = p.y + p.vy * dt
        p.vy = p.vy + 100 * dt
        p.life = p.life - dt
        if p.life <= 0 then
            table.remove(particles, i)
        end
    end
end

local function drawParticles()
    for _, p in ipairs(particles) do
        local alpha = p.life / 0.7
        local px = math.floor(p.x)
        local py = math.floor(p.y)
        if px >= 0 and px < WIDTH and py >= 0 and py < HEIGHT then
            setPixel(px, py, p.r * alpha, p.g * alpha, p.b * alpha)
        end
    end
end

---------------------------------------------------------
-- Hole / Tourist Management
---------------------------------------------------------

local function initHoles()
    holes = {}
    for i, pos in ipairs(holePositions) do
        table.insert(holes, {
            x = pos.x,
            y = pos.y,
            hasTourist = false,
            touristType = 1,
            popupProgress = 0,
            state = "hidden",
            stateTimer = 0,
            visibleDuration = 1.5,
            wasHit = false
        })
    end
end

local function spawnTouristInRandomHole()
    local emptyHoles = {}
    for i, hole in ipairs(holes) do
        if hole.state == "hidden" then
            table.insert(emptyHoles, i)
        end
    end

    if #emptyHoles > 0 then
        local idx = emptyHoles[math.random(#emptyHoles)]
        local hole = holes[idx]
        hole.hasTourist = true
        hole.touristType = math.random(1, 3)
        hole.state = "rising"
        hole.stateTimer = 0
        hole.popupProgress = 0
        hole.wasHit = false
        hole.visibleDuration = math.max(0.6, 2.0 - (60 - timeLeft) * 0.02)
    end
end

local function updateHoles(dt)
    for _, hole in ipairs(holes) do
        hole.stateTimer = hole.stateTimer + dt

        if hole.state == "rising" then
            hole.popupProgress = hole.popupProgress + dt * 5
            if hole.popupProgress >= 1 then
                hole.popupProgress = 1
                hole.state = "visible"
                hole.stateTimer = 0
            end
        elseif hole.state == "visible" then
            if hole.stateTimer >= hole.visibleDuration then
                hole.state = "falling"
                hole.stateTimer = 0
                combo = 0
            end
        elseif hole.state == "falling" then
            hole.popupProgress = hole.popupProgress - dt * 4
            if hole.popupProgress <= 0 then
                hole.popupProgress = 0
                hole.state = "hidden"
                hole.hasTourist = false
            end
        elseif hole.state == "hit" then
            hole.popupProgress = hole.popupProgress - dt * 8
            if hole.popupProgress <= 0 then
                hole.popupProgress = 0
                hole.state = "hidden"
                hole.hasTourist = false
            end
        end
    end
end

---------------------------------------------------------
-- Mallet / Hit Detection
---------------------------------------------------------

local function tryHitTourist()
    mallet.frame = 2
    mallet.hitTimer = mallet.hitDuration

    for _, hole in ipairs(holes) do
        if (hole.state == "visible" or hole.state == "rising") and hole.popupProgress > 0.5 then
            local dx = math.abs(mallet.x - hole.x)
            local dy = math.abs(mallet.y - (hole.y - 10 * hole.popupProgress))

            if dx < 18 and dy < 14 then
                hole.state = "hit"
                hole.wasHit = true
                combo = combo + 1
                comboTimer = 2

                local points = 100 * combo
                score = score + points

                spawnParticles(hole.x, hole.y - 8, YELLOW[1], YELLOW[2], YELLOW[3], 10)

                return true
            end
        end
    end

    return false
end

local function updateMallet(dt)
    if mallet.hitTimer > 0 then
        mallet.hitTimer = mallet.hitTimer - dt
        if mallet.hitTimer <= 0 then
            mallet.frame = 1
        end
    end
end

---------------------------------------------------------
-- AI Demo Mode
---------------------------------------------------------

local function findBestTarget()
    local bestHole = nil
    local bestScore = -1

    for i, hole in ipairs(holes) do
        if (hole.state == "visible" or hole.state == "rising") and hole.popupProgress > 0.3 then
            local urgency = hole.stateTimer
            local dx = math.abs(mallet.x - hole.x)
            local dy = math.abs(mallet.y - (hole.y - 8))
            local distance = math.sqrt(dx * dx + dy * dy)

            local targetScore = urgency * 10 - distance * 0.1

            if targetScore > bestScore then
                bestScore = targetScore
                bestHole = hole
            end
        end
    end

    return bestHole
end

local function updateAI(dt)
    if ai.reactionDelay > 0 then
        ai.reactionDelay = ai.reactionDelay - dt
        return
    end

    if ai.waitTimer > 0 then
        ai.waitTimer = ai.waitTimer - dt
        return
    end

    if mallet.hitTimer > 0 then
        return
    end

    if ai.targetHole == nil or ai.targetHole.state == "hidden" or ai.targetHole.state == "hit" then
        ai.targetHole = findBestTarget()
        if ai.targetHole then
            ai.reactionDelay = 0.05 + math.random() * 0.15
            ai.state = "moving"
        else
            ai.state = "idle"
            if math.random() < 0.02 then
                mallet.x = mallet.x + (math.random() - 0.5) * 8
                mallet.y = mallet.y + (math.random() - 0.5) * 8
            end
        end
        return
    end

    local targetX = ai.targetHole.x
    local targetY = ai.targetHole.y - 8

    if math.random() < 0.1 then
        targetX = targetX + (math.random() - 0.5) * 4
        targetY = targetY + (math.random() - 0.5) * 4
    end

    local dx = targetX - mallet.x
    local dy = targetY - mallet.y
    local distance = math.sqrt(dx * dx + dy * dy)

    if distance > 3 then
        local moveAmount = ai.moveSpeed * dt
        if moveAmount > distance then
            moveAmount = distance
        end

        mallet.x = mallet.x + (dx / distance) * moveAmount
        mallet.y = mallet.y + (dy / distance) * moveAmount
        ai.state = "moving"
    else
        ai.state = "hitting"

        if math.random() < ai.missChance then
            ai.waitTimer = 0.1 + math.random() * 0.2
            ai.targetHole = nil
        else
            tryHitTourist()
            ai.targetHole = nil
            ai.waitTimer = 0.1 + math.random() * 0.15
        end
    end

    mallet.x = math.max(12, math.min(WIDTH - 12, mallet.x))
    mallet.y = math.max(22, math.min(HEIGHT - 5, mallet.y))
end

---------------------------------------------------------
-- Drawing Functions
---------------------------------------------------------

local function drawBackground()
    -- Sky gradient (top)
    for y = 0, 17 do
        local blend = y / 17
        local r = 100 + blend * 50
        local g = 180 + blend * 40
        local b = 255 - blend * 30
        for x = 0, WIDTH - 1 do
            setPixel(x, y, r, g, b)
        end
    end

    -- Water
    for y = 18, 26 do
        local wave = math.sin(gameTime * 2 + y * 0.5) * 0.3 + 0.5
        for x = 0, WIDTH - 1 do
            local waveX = math.sin(gameTime * 1.5 + x * 0.1) * 0.2 + 0.5
            if (wave + waveX) > 0.7 then
                setPixel(x, y, WATER_LIGHT[1], WATER_LIGHT[2], WATER_LIGHT[3])
            else
                setPixel(x, y, WATER_DARK[1], WATER_DARK[2], WATER_DARK[3])
            end
        end
    end

    -- Beach sand
    for y = 27, HEIGHT - 1 do
        for x = 0, WIDTH - 1 do
            if (x + y) % 3 == 0 then
                setPixel(x, y, BEACH_DARK[1], BEACH_DARK[2], BEACH_DARK[3])
            else
                setPixel(x, y, BEACH_SAND[1], BEACH_SAND[2], BEACH_SAND[3])
            end
        end
    end

    -- Palm trees (left)
    for y = 8, 26 do
        setPixel(8, y, PALM_TRUNK[1], PALM_TRUNK[2], PALM_TRUNK[3])
        setPixel(9, y, PALM_TRUNK[1] + 20, PALM_TRUNK[2] + 15, PALM_TRUNK[3] + 10)
    end
    for i = -5, 5 do
        local leafY = 6 + math.abs(i) * 0.5
        setPixel(8 + i, math.floor(leafY), PALM_GREEN[1], PALM_GREEN[2], PALM_GREEN[3])
        setPixel(9 + i, math.floor(leafY) + 1, PALM_GREEN[1] - 10, PALM_GREEN[2] - 20, PALM_GREEN[3] - 10)
    end

    -- Palm trees (right)
    for y = 10, 26 do
        setPixel(118, y, PALM_TRUNK[1], PALM_TRUNK[2], PALM_TRUNK[3])
        setPixel(119, y, PALM_TRUNK[1] + 20, PALM_TRUNK[2] + 15, PALM_TRUNK[3] + 10)
    end
    for i = -5, 5 do
        local leafY = 8 + math.abs(i) * 0.5
        setPixel(118 + i, math.floor(leafY), PALM_GREEN[1], PALM_GREEN[2], PALM_GREEN[3])
        setPixel(119 + i, math.floor(leafY) + 1, PALM_GREEN[1] - 10, PALM_GREEN[2] - 20, PALM_GREEN[3] - 10)
    end
end

local function drawHole(hole)
    local hx = hole.x
    local hy = hole.y

    -- Draw hole (ellipse-ish)
    for dx = -10, 10 do
        for dy = -3, 3 do
            local dist = (dx * dx) / 100 + (dy * dy) / 9
            if dist < 1 then
                local px = hx + dx
                local py = hy + dy
                if px >= 0 and px < WIDTH and py >= 0 and py < HEIGHT then
                    if dist < 0.6 then
                        setPixel(px, py, HOLE_DARK[1], HOLE_DARK[2], HOLE_DARK[3])
                    else
                        setPixel(px, py, HOLE_LIGHT[1], HOLE_LIGHT[2], HOLE_LIGHT[3])
                    end
                end
            end
        end
    end
end

local function drawTourist(hole)
    if not hole.hasTourist or hole.popupProgress <= 0 then
        return
    end

    -- Get tourist sprite based on type
    local sprite, palette
    if hole.touristType == 1 and tourist1 and tourist1_palette then
        sprite, palette = tourist1, tourist1_palette
    elseif hole.touristType == 2 and tourist2 and tourist2_palette then
        sprite, palette = tourist2, tourist2_palette
    elseif hole.touristType == 3 and tourist3 and tourist3_palette then
        sprite, palette = tourist3, tourist3_palette
    end

    if not sprite then return end

    local spriteW = math.floor(32 * TOURIST_SCALE)
    local spriteH = math.floor(32 * TOURIST_SCALE)

    -- Position tourist (rises from hole)
    local riseAmount = hole.popupProgress * spriteH * 0.7
    local tx = hole.x - spriteW / 2
    local ty = hole.y - riseAmount
    local clipY = hole.y - 2

    if hole.wasHit then
        -- Flash red when hit
        drawSpriteTinted(sprite, palette, tx, ty, TOURIST_SCALE, true, clipY, 255, 100, 100)
    else
        drawSpriteDownscaled(sprite, palette, tx, ty, TOURIST_SCALE, true, clipY)
    end
end

local function drawMallet()
    local mx = math.floor(mallet.x)
    local my = math.floor(mallet.y)
    local hitting = mallet.frame == 2

    local sprite, palette

    -- Use run sprite for Spanish guy, or mallet sprites
    if hitting and mallet2 and mallet2_palette then
        sprite, palette = mallet2, mallet2_palette
    elseif mallet1 and mallet1_palette then
        sprite, palette = mallet1, mallet1_palette
    end

    if sprite then
        local spriteW = math.floor(48 * MALLET_SCALE)
        local spriteH = math.floor(48 * MALLET_SCALE)
        local drawX = mx - spriteW / 2
        local drawY = my - spriteH / 2

        if hitting then
            drawY = drawY + 3  -- Lower when hitting
        end

        drawSpriteDownscaled(sprite, palette, drawX, drawY, MALLET_SCALE, true, nil)
    end
end

---------------------------------------------------------
-- HUD Drawing
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

local function drawDigit(digit, startX, startY, r, g, b)
    local pattern = digits[digit]
    if not pattern then return end

    for row = 1, 5 do
        for col = 1, 3 do
            if pattern[row][col] == 1 then
                setPixel(startX + col - 1, startY + row - 1, r, g, b)
            end
        end
    end
end

local function drawNumber(num, startX, startY, r, g, b)
    local str = tostring(math.floor(num))
    local x = startX
    for i = 1, #str do
        local digit = tonumber(str:sub(i, i))
        drawDigit(digit, x, startY, r, g, b)
        x = x + 4
    end
end

local function drawHUD()
    -- Score
    drawNumber(score, 4, 2, YELLOW[1], YELLOW[2], YELLOW[3])

    -- Time remaining
    local timeColor = timeLeft < 10 and RED or GREEN
    drawNumber(math.ceil(timeLeft), WIDTH - 14, 2, timeColor[1], timeColor[2], timeColor[3])

    -- Combo indicator
    if combo > 1 and comboTimer > 0 then
        local comboX = WIDTH / 2 - 8
        drawNumber(combo, comboX, 10, YELLOW[1], YELLOW[2], YELLOW[3])
        -- Draw "x"
        setPixel(comboX + 6, 11, YELLOW[1], YELLOW[2], YELLOW[3])
        setPixel(comboX + 8, 11, YELLOW[1], YELLOW[2], YELLOW[3])
        setPixel(comboX + 7, 12, YELLOW[1], YELLOW[2], YELLOW[3])
        setPixel(comboX + 6, 13, YELLOW[1], YELLOW[2], YELLOW[3])
        setPixel(comboX + 8, 13, YELLOW[1], YELLOW[2], YELLOW[3])
    end
end

local function drawGameOver()
    -- Darken screen
    for y = 20, 44 do
        for x = 30, 98 do
            setPixel(x, y, 20, 20, 40)
        end
    end

    -- Border
    for x = 30, 98 do
        setPixel(x, 20, RED[1], RED[2], RED[3])
        setPixel(x, 44, RED[1], RED[2], RED[3])
    end
    for y = 20, 44 do
        setPixel(30, y, RED[1], RED[2], RED[3])
        setPixel(98, y, RED[1], RED[2], RED[3])
    end

    -- Score display
    drawNumber(score, 54, 30, YELLOW[1], YELLOW[2], YELLOW[3])

    -- Restart countdown
    local countdown = math.ceil(restartTimer)
    drawNumber(countdown, 62, 38, CYAN[1], CYAN[2], CYAN[3])
end

---------------------------------------------------------
-- Game Logic
---------------------------------------------------------

local function resetGame()
    score = 0
    timeLeft = 60
    combo = 0
    comboTimer = 0
    spawnTimer = 0
    spawnInterval = 1.5
    difficultyTimer = 0
    particles = {}
    gameState = "playing"
    restartTimer = 3

    ai.targetHole = nil
    ai.reactionDelay = 0
    ai.waitTimer = 0
    ai.state = "idle"
    ai.moveSpeed = 80 + math.random() * 40

    mallet.x = 64
    mallet.y = 40
    mallet.frame = 1
    mallet.hitTimer = 0

    initHoles()
end

---------------------------------------------------------
-- Main Functions
---------------------------------------------------------

local lastTime = 0
local initialized = false

function init(width, height)
    math.randomseed(os.time())
    loadSprites()
    initHoles()
    initialized = true
    lastTime = 0
    gameTime = 0
end

function reset()
    resetGame()
    lastTime = 0
    gameTime = 0
end

function update(audio, settings, time)
    if not initialized then
        init(WIDTH, HEIGHT)
    end

    local dt = time - lastTime
    if dt <= 0 or dt > 0.1 then dt = 0.016 end
    lastTime = time
    gameTime = gameTime + dt

    if gameState == "playing" then
        timeLeft = timeLeft - dt
        if timeLeft <= 0 then
            timeLeft = 0
            gameState = "gameover"
            restartTimer = 3
        else
            difficultyTimer = difficultyTimer + dt
            if difficultyTimer > 5 then
                difficultyTimer = 0
                spawnInterval = math.max(minSpawnInterval, spawnInterval - 0.1)
            end

            spawnTimer = spawnTimer + dt
            if spawnTimer >= spawnInterval then
                spawnTimer = 0
                spawnTouristInRandomHole()
                if spawnInterval < 0.8 and math.random() < 0.3 then
                    spawnTouristInRandomHole()
                end
            end

            if comboTimer > 0 then
                comboTimer = comboTimer - dt
                if comboTimer <= 0 then
                    combo = 0
                end
            end

            updateAI(dt)
            updateHoles(dt)
            updateMallet(dt)
        end
    else
        restartTimer = restartTimer - dt
        if restartTimer <= 0 then
            resetGame()
        end
    end

    updateParticles(dt)

    -- Draw everything
    clear()

    drawBackground()

    for _, hole in ipairs(holes) do
        drawHole(hole)
    end

    for _, hole in ipairs(holes) do
        drawTourist(hole)
    end

    drawParticles()
    drawMallet()
    drawHUD()

    if gameState == "gameover" then
        drawGameOver()
    end
end
