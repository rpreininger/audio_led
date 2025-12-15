---------------------------------------------------------
-- Donkey Kong LED Effect - 128x64
-- Self-running demo with AI player
---------------------------------------------------------

effect_name = "Donkey Kong"
effect_description = "Classic arcade game demo with AI player"

---------------------------------------------------------
-- Colors (Classic Donkey Kong style)
---------------------------------------------------------

local RED = {255, 50, 50}
local PINK = {255, 150, 180}
local BLUE = {50, 100, 255}
local CYAN = {0, 220, 255}
local WHITE = {255, 255, 255}
local YELLOW = {255, 220, 50}
local ORANGE = {255, 140, 0}
local BROWN = {139, 69, 19}
local DARK_RED = {100, 20, 20}

---------------------------------------------------------
-- Game State
---------------------------------------------------------

local gameState = "playing"
local score = 0
local lives = 3
local level = 1
local gameTime = 0
local bonusTimer = 5000
local restartTimer = 0

-- Player (Mario) - AI controlled
local player = {
    x = 10,
    y = 56,
    vx = 0,
    vy = 0,
    width = 4,
    height = 6,
    onGround = false,
    onLadder = false,
    climbing = false,
    direction = 1,
    animFrame = 0,
    hasHammer = false,
    hammerTimer = 0,
    -- AI state
    aiTarget = nil,
    aiState = "run",
    aiTimer = 0,
    currentPlatform = 1,
    targetLadder = nil,
    chosenLadder = nil,
    waitTimer = 0,
    moveSpeed = 30
}

-- Donkey Kong
local donkeyKong = {
    x = 8,
    y = 4,
    animFrame = 0,
    animTimer = 0,
    throwTimer = 0,
    throwInterval = 2.0
}

-- Pauline
local pauline = {
    x = 56,
    y = 2,
    helpTimer = 0
}

-- Level data
local platforms = {}
local ladders = {}
local barrels = {}
local particles = {}

---------------------------------------------------------
-- Level Setup
---------------------------------------------------------

local function initLevel()
    platforms = {}
    ladders = {}
    barrels = {}
    particles = {}

    -- Platforms - y is where player stands
    table.insert(platforms, {x = 0, y = 58, width = 128, height = 2, slope = 0, id = 1})
    table.insert(platforms, {x = 8, y = 48, width = 112, height = 2, slope = 0.02, id = 2})
    table.insert(platforms, {x = 8, y = 38, width = 112, height = 2, slope = -0.02, id = 3})
    table.insert(platforms, {x = 8, y = 28, width = 112, height = 2, slope = 0.02, id = 4})
    table.insert(platforms, {x = 8, y = 18, width = 112, height = 2, slope = -0.02, id = 5})
    table.insert(platforms, {x = 0, y = 10, width = 75, height = 2, slope = 0, id = 6})

    -- Ladders
    table.insert(ladders, {x = 100, y = 48, height = 10, fromPlat = 1, toPlat = 2})
    table.insert(ladders, {x = 20, y = 38, height = 10, fromPlat = 2, toPlat = 3})
    table.insert(ladders, {x = 80, y = 38, height = 10, fromPlat = 2, toPlat = 3})
    table.insert(ladders, {x = 100, y = 28, height = 10, fromPlat = 3, toPlat = 4})
    table.insert(ladders, {x = 40, y = 28, height = 10, fromPlat = 3, toPlat = 4})
    table.insert(ladders, {x = 20, y = 18, height = 10, fromPlat = 4, toPlat = 5})
    table.insert(ladders, {x = 80, y = 18, height = 10, fromPlat = 4, toPlat = 5})
    table.insert(ladders, {x = 65, y = 10, height = 8, fromPlat = 5, toPlat = 6})

    -- Reset player
    player.x = 5 + math.random(10)
    player.y = 58
    player.vx = 0
    player.vy = 0
    player.onGround = true
    player.onLadder = false
    player.climbing = false
    player.hasHammer = false
    player.aiState = "run"
    player.aiTimer = 0
    player.currentPlatform = 1
    player.direction = 1
    player.targetLadder = nil
    player.chosenLadder = nil
    player.waitTimer = 0
    player.moveSpeed = 25 + math.random(15)

    -- Reset DK
    donkeyKong.x = 8
    donkeyKong.y = 4
    donkeyKong.throwTimer = 1.0

    bonusTimer = 5000
    gameState = "playing"
end

---------------------------------------------------------
-- Particles
---------------------------------------------------------

local function spawnParticles(x, y, r, g, b, count)
    for i = 1, count do
        table.insert(particles, {
            x = x,
            y = y,
            vx = (math.random() - 0.5) * 40,
            vy = (math.random() - 0.5) * 40 - 20,
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
        p.vy = p.vy + 80 * dt
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
-- Drawing Functions
---------------------------------------------------------

local function drawPlatforms()
    for _, plat in ipairs(platforms) do
        for x = 0, plat.width - 1 do
            local px = plat.x + x
            local py = plat.y + math.floor(x * plat.slope)

            for h = 0, plat.height - 1 do
                if (x + h) % 4 < 2 then
                    setPixel(px, py + h, RED[1], RED[2], RED[3])
                else
                    setPixel(px, py + h, DARK_RED[1], DARK_RED[2], DARK_RED[3])
                end
            end
        end
    end
end

local function drawLadders()
    for _, ladder in ipairs(ladders) do
        for y = 0, ladder.height - 1 do
            setPixel(ladder.x, ladder.y + y, CYAN[1], CYAN[2], CYAN[3])
            setPixel(ladder.x + 3, ladder.y + y, CYAN[1], CYAN[2], CYAN[3])

            if y % 3 == 0 then
                setPixel(ladder.x + 1, ladder.y + y, CYAN[1], CYAN[2], CYAN[3])
                setPixel(ladder.x + 2, ladder.y + y, CYAN[1], CYAN[2], CYAN[3])
            end
        end
    end
end

local function drawDonkeyKong()
    local dx = donkeyKong.x
    local dy = donkeyKong.y
    local frame = math.floor(donkeyKong.animFrame) % 4
    local throwing = donkeyKong.throwTimer < 0.5

    -- Body (brown)
    for y = 0, 5 do
        for x = 0, 7 do
            local isBody = false

            if y == 0 then
                isBody = x >= 2 and x <= 5
            elseif y == 1 then
                isBody = x >= 1 and x <= 6
            elseif y == 2 then
                isBody = x >= 0 and x <= 7
            elseif y == 3 then
                isBody = x >= 1 and x <= 6
            elseif y == 4 then
                isBody = x >= 1 and x <= 6
            elseif y == 5 then
                if frame == 0 or frame == 2 then
                    isBody = (x >= 1 and x <= 2) or (x >= 5 and x <= 6)
                else
                    isBody = (x >= 0 and x <= 2) or (x >= 5 and x <= 7)
                end
            end

            if isBody then
                setPixel(dx + x, dy + y, BROWN[1] + 60, BROWN[2] + 40, BROWN[3] + 10)
            end
        end
    end

    -- Arms
    if throwing then
        setPixel(dx + 8, dy + 1, BROWN[1] + 60, BROWN[2] + 40, BROWN[3] + 10)
        setPixel(dx + 9, dy + 1, BROWN[1] + 60, BROWN[2] + 40, BROWN[3] + 10)
        setPixel(dx + 8, dy + 2, BROWN[1] + 60, BROWN[2] + 40, BROWN[3] + 10)
        setPixel(dx + 9, dy + 2, ORANGE[1], ORANGE[2], ORANGE[3])
        setPixel(dx + 10, dy + 2, ORANGE[1], ORANGE[2], ORANGE[3])
        setPixel(dx + 9, dy + 3, ORANGE[1], ORANGE[2], ORANGE[3])
        setPixel(dx + 10, dy + 3, ORANGE[1], ORANGE[2], ORANGE[3])
    else
        if frame == 0 or frame == 2 then
            setPixel(dx - 1, dy + 2, BROWN[1] + 60, BROWN[2] + 40, BROWN[3] + 10)
            setPixel(dx + 8, dy + 2, BROWN[1] + 60, BROWN[2] + 40, BROWN[3] + 10)
        else
            setPixel(dx - 1, dy + 3, BROWN[1] + 60, BROWN[2] + 40, BROWN[3] + 10)
            setPixel(dx + 8, dy + 3, BROWN[1] + 60, BROWN[2] + 40, BROWN[3] + 10)
        end
    end

    -- Face
    setPixel(dx + 2, dy + 1, 200, 150, 100)
    setPixel(dx + 3, dy + 1, 200, 150, 100)
    setPixel(dx + 4, dy + 1, 200, 150, 100)
    setPixel(dx + 5, dy + 1, 200, 150, 100)
    setPixel(dx + 2, dy, WHITE[1], WHITE[2], WHITE[3])
    setPixel(dx + 5, dy, WHITE[1], WHITE[2], WHITE[3])
    setPixel(dx + 3, dy + 2, 80, 40, 20)
    setPixel(dx + 4, dy + 2, 80, 40, 20)
end

local function drawPauline()
    local px = pauline.x
    local py = pauline.y

    setPixel(px, py, YELLOW[1], YELLOW[2], YELLOW[3])
    setPixel(px + 1, py, YELLOW[1], YELLOW[2], YELLOW[3])
    setPixel(px + 2, py, YELLOW[1], YELLOW[2], YELLOW[3])
    setPixel(px + 1, py + 1, 255, 200, 180)

    for y = 2, 5 do
        local w = 1
        if y >= 4 then w = 2 end
        for x = -w, w do
            setPixel(px + 1 + x, py + y, PINK[1], PINK[2], PINK[3])
        end
    end

    if math.floor(pauline.helpTimer * 3) % 2 == 0 then
        setPixel(px - 3, py - 1, WHITE[1], WHITE[2], WHITE[3])
        setPixel(px + 5, py - 1, WHITE[1], WHITE[2], WHITE[3])
    end
end

local function drawPlayer()
    local px = math.floor(player.x)
    local py = math.floor(player.y)
    local frame = math.floor(player.animFrame) % 4

    if player.climbing then
        local climbFrame = math.floor(player.animFrame) % 2

        setPixel(px, py - 5, RED[1], RED[2], RED[3])
        setPixel(px + 1, py - 5, RED[1], RED[2], RED[3])
        setPixel(px + 2, py - 5, RED[1], RED[2], RED[3])
        setPixel(px + 1, py - 4, 255, 200, 180)
        setPixel(px + 1, py - 3, BLUE[1], BLUE[2], BLUE[3])
        setPixel(px + 1, py - 2, BLUE[1], BLUE[2], BLUE[3])

        if climbFrame == 0 then
            setPixel(px - 1, py - 3, RED[1], RED[2], RED[3])
            setPixel(px + 3, py - 2, RED[1], RED[2], RED[3])
        else
            setPixel(px - 1, py - 2, RED[1], RED[2], RED[3])
            setPixel(px + 3, py - 3, RED[1], RED[2], RED[3])
        end

        setPixel(px, py - 1, BLUE[1], BLUE[2], BLUE[3])
        setPixel(px + 2, py - 1, BLUE[1], BLUE[2], BLUE[3])
        setPixel(px, py, BROWN[1], BROWN[2], BROWN[3])
        setPixel(px + 2, py, BROWN[1], BROWN[2], BROWN[3])
    else
        setPixel(px, py - 5, RED[1], RED[2], RED[3])
        setPixel(px + 1, py - 5, RED[1], RED[2], RED[3])
        if player.direction == 1 then
            setPixel(px + 2, py - 5, RED[1], RED[2], RED[3])
        else
            setPixel(px - 1, py - 5, RED[1], RED[2], RED[3])
        end

        setPixel(px + 1, py - 4, 255, 200, 180)
        setPixel(px + player.direction, py - 4, 255, 200, 180)
        setPixel(px + 1, py - 3, BLUE[1], BLUE[2], BLUE[3])
        setPixel(px, py - 3, RED[1], RED[2], RED[3])
        setPixel(px + 2, py - 3, RED[1], RED[2], RED[3])
        setPixel(px + 1, py - 2, BLUE[1], BLUE[2], BLUE[3])
        setPixel(px, py - 2, BLUE[1], BLUE[2], BLUE[3])
        setPixel(px + 2, py - 2, BLUE[1], BLUE[2], BLUE[3])

        if not player.onGround then
            setPixel(px, py - 1, BLUE[1], BLUE[2], BLUE[3])
            setPixel(px + 2, py - 1, BLUE[1], BLUE[2], BLUE[3])
            setPixel(px - 1, py, BROWN[1], BROWN[2], BROWN[3])
            setPixel(px + 3, py, BROWN[1], BROWN[2], BROWN[3])
        elseif frame == 0 or frame == 2 then
            setPixel(px, py - 1, BLUE[1], BLUE[2], BLUE[3])
            setPixel(px + 2, py - 1, BLUE[1], BLUE[2], BLUE[3])
            setPixel(px, py, BROWN[1], BROWN[2], BROWN[3])
            setPixel(px + 2, py, BROWN[1], BROWN[2], BROWN[3])
        else
            setPixel(px - 1, py - 1, BLUE[1], BLUE[2], BLUE[3])
            setPixel(px + 3, py - 1, BLUE[1], BLUE[2], BLUE[3])
            setPixel(px - 1, py, BROWN[1], BROWN[2], BROWN[3])
            setPixel(px + 3, py, BROWN[1], BROWN[2], BROWN[3])
        end
    end
end

local function drawBarrel(barrel)
    local bx = math.floor(barrel.x)
    local by = math.floor(barrel.y)
    local frame = math.floor(barrel.rotation) % 4

    for y = 0, 3 do
        for x = 0, 3 do
            local isBarrel = not ((x == 0 and y == 0) or (x == 3 and y == 0) or
                                   (x == 0 and y == 3) or (x == 3 and y == 3))

            if isBarrel then
                local stripe = ((x + y + frame) % 2 == 0)
                if stripe then
                    setPixel(bx + x, by + y, ORANGE[1], ORANGE[2], ORANGE[3])
                else
                    setPixel(bx + x, by + y, BROWN[1] + 20, BROWN[2] + 20, BROWN[3] + 20)
                end
            end
        end
    end
end

local function drawBarrels()
    for _, barrel in ipairs(barrels) do
        drawBarrel(barrel)
    end
end

---------------------------------------------------------
-- HUD
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

local function drawHUD()
    local scoreStr = string.format("%06d", score)
    local x = 90
    for i = 1, #scoreStr do
        local digit = tonumber(scoreStr:sub(i, i))
        drawDigit(digit, x, 2, CYAN[1], CYAN[2], CYAN[3])
        x = x + 5
    end

    for i = 1, lives do
        setPixel(118 + i * 3, 58, RED[1], RED[2], RED[3])
        setPixel(119 + i * 3, 58, RED[1], RED[2], RED[3])
    end
end

---------------------------------------------------------
-- Physics
---------------------------------------------------------

local gravity = 200
local jumpStrength = -85
local climbSpeed = 25

local function findLaddersOnPlatform(platId)
    local result = {}
    for _, ladder in ipairs(ladders) do
        if ladder.fromPlat == platId then
            table.insert(result, ladder)
        end
    end
    return result
end

local function findNearestLadderUp()
    local availableLadders = findLaddersOnPlatform(player.currentPlatform)

    if #availableLadders == 0 then
        return nil
    elseif #availableLadders == 1 then
        return availableLadders[1]
    else
        if not player.chosenLadder or player.chosenLadder.fromPlat ~= player.currentPlatform then
            player.chosenLadder = availableLadders[math.random(#availableLadders)]
        end
        return player.chosenLadder
    end
end

local function isBarrelNear(checkX, checkY, radius)
    for _, barrel in ipairs(barrels) do
        local dx = math.abs(barrel.x - checkX)
        local dy = math.abs(barrel.y - checkY)
        if dx < radius and dy < 10 then
            return barrel
        end
    end
    return nil
end

---------------------------------------------------------
-- AI Player
---------------------------------------------------------

local function updateAI(dt)
    player.aiTimer = player.aiTimer + dt

    local nearBarrel = isBarrelNear(player.x + player.direction * 15, player.y, 20)

    if nearBarrel and player.onGround and not player.climbing then
        player.vy = jumpStrength
        player.onGround = false
        spawnParticles(player.x + 1, player.y, WHITE[1], WHITE[2], WHITE[3], 3)
        score = score + 100
        return
    end

    local targetLadder = findNearestLadderUp()

    if player.climbing and player.targetLadder then
        player.y = player.y - climbSpeed * dt
        player.animFrame = player.animFrame + dt * 8

        if player.y <= player.targetLadder.y then
            player.y = player.targetLadder.y
            player.climbing = false
            player.onLadder = false
            player.onGround = true
            player.currentPlatform = player.targetLadder.toPlat
            player.targetLadder = nil
            spawnParticles(player.x + 1, player.y, CYAN[1], CYAN[2], CYAN[3], 3)
        end
    elseif targetLadder then
        local ladderCenterX = targetLadder.x + 1

        if player.waitTimer > 0 then
            player.waitTimer = player.waitTimer - dt
            return
        end

        if math.random() < 0.005 then
            player.waitTimer = 0.2 + math.random() * 0.3
            return
        end

        if math.abs(player.x - ladderCenterX) < 5 then
            player.climbing = true
            player.onLadder = true
            player.onGround = false
            player.x = ladderCenterX
            player.vy = 0
            player.targetLadder = targetLadder
            player.chosenLadder = nil
        else
            if player.x < ladderCenterX then
                player.x = player.x + player.moveSpeed * dt
                player.direction = 1
            else
                player.x = player.x - player.moveSpeed * dt
                player.direction = -1
            end
            player.animFrame = player.animFrame + dt * 8
        end
    else
        if player.x < pauline.x - 5 then
            player.x = player.x + player.moveSpeed * dt
            player.direction = 1
            player.animFrame = player.animFrame + dt * 8
        elseif player.x > pauline.x + 5 then
            player.x = player.x - player.moveSpeed * dt
            player.direction = -1
            player.animFrame = player.animFrame + dt * 8
        end
    end
end

local function updatePlayer(dt)
    updateAI(dt)

    if player.climbing then
        return
    end

    if not player.onLadder then
        player.vy = player.vy + gravity * dt
        player.y = player.y + player.vy * dt
    end

    player.onGround = false
    for _, plat in ipairs(platforms) do
        if player.x + 3 > plat.x and player.x < plat.x + plat.width then
            local localX = math.max(0, player.x - plat.x)
            local platY = plat.y + math.floor(localX * plat.slope)

            if player.y >= platY - 2 and player.y <= platY + 3 and player.vy >= 0 then
                player.y = platY
                player.vy = 0
                player.onGround = true
                player.currentPlatform = plat.id
            end
        end
    end

    player.x = math.max(0, math.min(WIDTH - 4, player.x))

    if player.y > HEIGHT + 10 then
        lives = lives - 1
        spawnParticles(player.x, HEIGHT - 5, RED[1], RED[2], RED[3], 10)
        if lives <= 0 then
            gameState = "lost"
            restartTimer = 3
        else
            player.x = 10
            player.y = 58
            player.vy = 0
            player.currentPlatform = 1
            player.climbing = false
            player.onLadder = false
        end
    end

    if player.currentPlatform >= 6 and player.y < 12 and math.abs(player.x - pauline.x) < 10 then
        gameState = "won"
        score = score + math.floor(bonusTimer)
        restartTimer = 3
    end
end

local function spawnBarrel()
    local barrel = {
        x = donkeyKong.x + 8,
        y = donkeyKong.y + 4,
        vx = 25,
        vy = 0,
        rotation = 0,
        onGround = false,
        platformId = 6
    }
    table.insert(barrels, barrel)
end

local function updateDonkeyKong(dt)
    donkeyKong.animTimer = donkeyKong.animTimer + dt
    if donkeyKong.animTimer > 0.3 then
        donkeyKong.animTimer = 0
        donkeyKong.animFrame = donkeyKong.animFrame + 1
    end

    donkeyKong.throwTimer = donkeyKong.throwTimer - dt
    if donkeyKong.throwTimer <= 0 then
        spawnBarrel()
        donkeyKong.throwTimer = 1.2 + math.random() * 1.3
    end
end

local function updateBarrels(dt)
    local clearAllBarrels = false

    for i = #barrels, 1, -1 do
        local barrel = barrels[i]
        if not barrel then break end

        barrel.rotation = barrel.rotation + math.abs(barrel.vx) * dt * 0.3
        barrel.vy = barrel.vy + gravity * dt
        barrel.x = barrel.x + barrel.vx * dt
        barrel.y = barrel.y + barrel.vy * dt

        barrel.onGround = false
        for _, plat in ipairs(platforms) do
            if barrel.x + 3 > plat.x and barrel.x < plat.x + plat.width then
                local localX = math.max(0, barrel.x - plat.x)
                local platY = plat.y + math.floor(localX * plat.slope)

                if barrel.y + 3 >= platY and barrel.y < platY + 4 and barrel.vy >= 0 then
                    barrel.y = platY - 3
                    barrel.vy = 0
                    barrel.onGround = true
                    barrel.platformId = plat.id

                    if plat.slope > 0 then
                        barrel.vx = 25
                    elseif plat.slope < 0 then
                        barrel.vx = -25
                    else
                        if barrel.vx == 0 then barrel.vx = 25 end
                    end
                end
            end
        end

        if barrel.onGround and math.random() < 0.015 then
            for _, ladder in ipairs(ladders) do
                if ladder.toPlat == barrel.platformId then
                    if math.abs(barrel.x + 2 - ladder.x - 2) < 6 then
                        barrel.vy = 30
                        barrel.vx = 0
                        barrel.onGround = false
                        break
                    end
                end
            end
        end

        local removeBarrel = false
        if barrel.x < -10 or barrel.x > WIDTH + 10 or barrel.y > HEIGHT + 10 then
            removeBarrel = true
        else
            local dx = math.abs(barrel.x + 2 - player.x - 1)
            local dy = math.abs(barrel.y + 2 - player.y + 2)

            if dx < 5 and dy < 6 then
                if not player.climbing then
                    lives = lives - 1
                    spawnParticles(player.x + 1, player.y - 3, RED[1], RED[2], RED[3], 10)
                    removeBarrel = true

                    if lives <= 0 then
                        gameState = "lost"
                        restartTimer = 3
                    else
                        player.x = 10
                        player.y = 58
                        player.vy = 0
                        player.currentPlatform = 1
                        player.climbing = false
                        player.onLadder = false
                        player.targetLadder = nil
                        clearAllBarrels = true
                    end
                end
            end
        end

        if removeBarrel then
            table.remove(barrels, i)
        end

        if clearAllBarrels then
            break
        end
    end

    if clearAllBarrels then
        barrels = {}
    end
end

local function updatePauline(dt)
    pauline.helpTimer = pauline.helpTimer + dt
end

---------------------------------------------------------
-- Main Functions
---------------------------------------------------------

local lastTime = 0
local initialized = false

function init(width, height)
    math.randomseed(os.time())
    initLevel()
    initialized = true
    lastTime = 0
end

function reset()
    score = 0
    lives = 3
    initLevel()
    lastTime = 0
end

function update(audio, settings, time)
    if not initialized then
        init(WIDTH, HEIGHT)
    end

    -- Calculate dt from time
    local dt = time - lastTime
    if dt <= 0 or dt > 0.1 then dt = 0.016 end
    lastTime = time

    -- Update game state
    if gameState == "playing" then
        gameTime = gameTime + dt
        bonusTimer = math.max(0, bonusTimer - dt * 5)

        updatePlayer(dt)
        updateDonkeyKong(dt)
        updateBarrels(dt)
        updatePauline(dt)
        updateParticles(dt)
    else
        restartTimer = restartTimer - dt
        updateParticles(dt)
        if restartTimer <= 0 then
            score = 0
            lives = 3
            initLevel()
        end
    end

    -- Draw everything
    clear()

    drawPlatforms()
    drawLadders()
    drawBarrels()
    drawDonkeyKong()
    drawPauline()
    drawPlayer()
    drawParticles()
    drawHUD()

    if gameState == "lost" then
        local flash = math.floor(gameTime * 4) % 2
        if flash == 0 then
            for x = 45, 83 do
                setPixel(x, 28, RED[1], RED[2], RED[3])
                setPixel(x, 36, RED[1], RED[2], RED[3])
            end
            for y = 28, 36 do
                setPixel(45, y, RED[1], RED[2], RED[3])
                setPixel(83, y, RED[1], RED[2], RED[3])
            end
        end
    elseif gameState == "won" then
        local flash = math.floor(gameTime * 4) % 2
        if flash == 0 then
            for x = 45, 83 do
                setPixel(x, 28, CYAN[1], CYAN[2], CYAN[3])
                setPixel(x, 36, CYAN[1], CYAN[2], CYAN[3])
            end
            for y = 28, 36 do
                setPixel(45, y, CYAN[1], CYAN[2], CYAN[3])
                setPixel(83, y, CYAN[1], CYAN[2], CYAN[3])
            end
        end
    end
end
