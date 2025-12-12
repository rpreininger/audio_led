-- ====================================================================
-- JUMPNRUN - Neon Platformer LED Effect
-- Autonomous AI-controlled endless runner visualization
-- ====================================================================

effect_name = "Jump'n'Run"
effect_description = "AI neon platformer with auto-jumping"

---------------------------------------------------------
-- Colors (Neon style)
---------------------------------------------------------

local CYAN = {0, 220, 255}
local MAGENTA = {255, 50, 200}
local WHITE = {255, 255, 255}
local DARK_CYAN = {0, 80, 100}

---------------------------------------------------------
-- Game State
---------------------------------------------------------

local scrollX = 0
local scrollSpeed = 25

local player = {}
local platforms = {}
local diamonds = {}
local spikes = {}
local particles = {}

local score = 0
local lastTime = 0

---------------------------------------------------------
-- Level Generation
---------------------------------------------------------

local function generatePlatform(startX)
    local lastY = 45
    local x = startX

    while x < startX + WIDTH * 2 do
        local pWidth = math.random(15, 35)
        local pY = lastY + math.random(-10, 10)
        pY = math.max(25, math.min(55, pY))

        table.insert(platforms, {
            x = x,
            y = pY,
            width = pWidth,
            height = 3
        })

        -- Add diamonds above platform
        if math.random() > 0.4 then
            local numDiamonds = math.random(1, 3)
            for i = 1, numDiamonds do
                table.insert(diamonds, {
                    x = x + (pWidth / (numDiamonds + 1)) * i,
                    y = pY - 10 - math.random(0, 5),
                    collected = false,
                    phase = math.random() * math.pi * 2
                })
            end
        end

        -- Add spikes on some platforms
        if math.random() > 0.7 then
            local spikeX = x + math.random(3, pWidth - 6)
            table.insert(spikes, {
                x = spikeX,
                y = pY - 4
            })
        end

        lastY = pY
        x = x + pWidth + math.random(8, 20)
    end
end

local function initLevel()
    platforms = {}
    diamonds = {}
    spikes = {}

    -- Starting platform
    table.insert(platforms, {
        x = 0,
        y = 50,
        width = 40,
        height = 3
    })

    generatePlatform(45)
end

local function cleanupAndGenerate()
    -- Remove off-screen elements
    for i = #platforms, 1, -1 do
        if platforms[i].x + platforms[i].width < scrollX - 20 then
            table.remove(platforms, i)
        end
    end

    for i = #diamonds, 1, -1 do
        if diamonds[i].x < scrollX - 20 then
            table.remove(diamonds, i)
        end
    end

    for i = #spikes, 1, -1 do
        if spikes[i].x < scrollX - 20 then
            table.remove(spikes, i)
        end
    end

    -- Generate new platforms ahead
    local maxX = 0
    for _, p in ipairs(platforms) do
        maxX = math.max(maxX, p.x + p.width)
    end

    if maxX < scrollX + WIDTH * 2 then
        generatePlatform(maxX + 10)
    end
end

---------------------------------------------------------
-- Particles
---------------------------------------------------------

local function spawnParticles(x, y, r, g, b, count)
    for i = 1, count do
        table.insert(particles, {
            x = x,
            y = y,
            vx = (math.random() - 0.5) * 30,
            vy = (math.random() - 0.5) * 30,
            life = 0.5 + math.random() * 0.3,
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
        p.life = p.life - dt
        if p.life <= 0 then
            table.remove(particles, i)
        end
    end
end

local function drawParticles()
    for _, p in ipairs(particles) do
        local alpha = p.life / 0.8
        local px = math.floor(p.x - scrollX)
        local py = math.floor(p.y)
        if px >= 0 and px < WIDTH and py >= 0 and py < HEIGHT then
            setPixel(px, py, math.floor(p.r * alpha), math.floor(p.g * alpha), math.floor(p.b * alpha))
        end
    end
end

---------------------------------------------------------
-- Drawing Functions
---------------------------------------------------------

local function drawBackground()
    -- Diagonal line pattern (subtle)
    for y = 0, HEIGHT - 1 do
        for x = 0, WIDTH - 1 do
            if (x + y + math.floor(scrollX * 0.1)) % 8 == 0 then
                setPixel(x, y, 15, 20, 25)
            end
        end
    end
end

local function drawPlatform(px, py, pwidth, pheight)
    local screenX = math.floor(px - scrollX)

    -- Main platform (cyan glow)
    for y = 0, pheight - 1 do
        for x = 0, pwidth - 1 do
            local sx = screenX + x
            local sy = py + y
            if sx >= 0 and sx < WIDTH then
                if y == 0 then
                    setPixel(sx, sy, CYAN[1], CYAN[2], CYAN[3])
                else
                    setPixel(sx, sy, DARK_CYAN[1], DARK_CYAN[2], DARK_CYAN[3])
                end
            end
        end
    end
end

local function drawPlatforms()
    for _, p in ipairs(platforms) do
        if p.x + p.width > scrollX - 10 and p.x < scrollX + WIDTH + 10 then
            drawPlatform(p.x, p.y, p.width, p.height)
        end
    end
end

local function drawDiamond(dx, dy, phase)
    local screenX = math.floor(dx - scrollX)
    local pulse = math.sin(phase) * 0.3 + 0.7

    local r = math.floor(MAGENTA[1] * pulse)
    local g = math.floor(MAGENTA[2] * pulse)
    local b = math.floor(MAGENTA[3] * pulse)

    if screenX >= 0 and screenX < WIDTH then
        -- Center
        setPixel(screenX, dy, r, g, b)

        -- Diamond pattern
        if screenX - 1 >= 0 then setPixel(screenX - 1, dy, r, g, b) end
        if screenX + 1 < WIDTH then setPixel(screenX + 1, dy, r, g, b) end
        setPixel(screenX, dy - 1, r, g, b)
        setPixel(screenX, dy + 1, r, g, b)

        -- Outer glow
        local r2, g2, b2 = math.floor(r * 0.5), math.floor(g * 0.5), math.floor(b * 0.5)
        if screenX - 2 >= 0 then setPixel(screenX - 2, dy, r2, g2, b2) end
        if screenX + 2 < WIDTH then setPixel(screenX + 2, dy, r2, g2, b2) end
        setPixel(screenX, dy - 2, r2, g2, b2)
        setPixel(screenX, dy + 2, r2, g2, b2)
    end
end

local function drawDiamonds()
    for _, d in ipairs(diamonds) do
        if not d.collected and d.x > scrollX - 10 and d.x < scrollX + WIDTH + 10 then
            d.phase = d.phase + 0.1
            drawDiamond(d.x, d.y, d.phase)
        end
    end
end

local function drawSpike(sx, sy)
    local screenX = math.floor(sx - scrollX)

    if screenX >= 0 and screenX < WIDTH then
        -- Triangle spike pointing up
        setPixel(screenX, sy, CYAN[1], CYAN[2], CYAN[3])
        if screenX - 1 >= 0 then setPixel(screenX - 1, sy + 1, CYAN[1], CYAN[2], CYAN[3]) end
        setPixel(screenX, sy + 1, CYAN[1], CYAN[2], CYAN[3])
        if screenX + 1 < WIDTH then setPixel(screenX + 1, sy + 1, CYAN[1], CYAN[2], CYAN[3]) end
        if screenX - 2 >= 0 then setPixel(screenX - 2, sy + 2, CYAN[1], CYAN[2], CYAN[3]) end
        if screenX - 1 >= 0 then setPixel(screenX - 1, sy + 2, CYAN[1], CYAN[2], CYAN[3]) end
        setPixel(screenX, sy + 2, CYAN[1], CYAN[2], CYAN[3])
        if screenX + 1 < WIDTH then setPixel(screenX + 1, sy + 2, CYAN[1], CYAN[2], CYAN[3]) end
        if screenX + 2 < WIDTH then setPixel(screenX + 2, sy + 2, CYAN[1], CYAN[2], CYAN[3]) end
    end
end

local function drawSpikes()
    for _, s in ipairs(spikes) do
        if s.x > scrollX - 10 and s.x < scrollX + WIDTH + 10 then
            drawSpike(s.x, s.y)
        end
    end
end

local function drawPlayer()
    local screenX = math.floor(player.x - scrollX)
    local py = math.floor(player.y)

    -- Running animation frame
    local frame = math.floor(player.runPhase) % 4

    -- Head
    setPixel(screenX, py - 7, WHITE[1], WHITE[2], WHITE[3])
    setPixel(screenX + 1, py - 7, WHITE[1], WHITE[2], WHITE[3])
    setPixel(screenX, py - 6, WHITE[1], WHITE[2], WHITE[3])
    setPixel(screenX + 1, py - 6, WHITE[1], WHITE[2], WHITE[3])

    -- Body
    setPixel(screenX, py - 5, CYAN[1], CYAN[2], CYAN[3])
    setPixel(screenX, py - 4, CYAN[1], CYAN[2], CYAN[3])
    setPixel(screenX, py - 3, CYAN[1], CYAN[2], CYAN[3])

    -- Arms (animated)
    if player.jumping then
        -- Arms up when jumping
        if screenX - 1 >= 0 then setPixel(screenX - 1, py - 5, CYAN[1], CYAN[2], CYAN[3]) end
        if screenX + 1 < WIDTH then setPixel(screenX + 1, py - 5, CYAN[1], CYAN[2], CYAN[3]) end
        if screenX - 2 >= 0 then setPixel(screenX - 2, py - 6, CYAN[1], CYAN[2], CYAN[3]) end
        if screenX + 2 < WIDTH then setPixel(screenX + 2, py - 6, CYAN[1], CYAN[2], CYAN[3]) end
    else
        -- Running arm swing
        local armOffset = (frame < 2) and 1 or -1
        if screenX - 1 >= 0 then setPixel(screenX - 1, py - 4 + armOffset, CYAN[1], CYAN[2], CYAN[3]) end
        if screenX + 1 < WIDTH then setPixel(screenX + 1, py - 4 - armOffset, CYAN[1], CYAN[2], CYAN[3]) end
    end

    -- Legs (animated)
    if player.jumping then
        -- Legs tucked when jumping
        if screenX - 1 >= 0 then setPixel(screenX - 1, py - 1, CYAN[1], CYAN[2], CYAN[3]) end
        if screenX + 1 < WIDTH then setPixel(screenX + 1, py - 1, CYAN[1], CYAN[2], CYAN[3]) end
    else
        -- Running leg animation
        if frame == 0 then
            if screenX - 1 >= 0 then
                setPixel(screenX - 1, py - 2, CYAN[1], CYAN[2], CYAN[3])
                setPixel(screenX - 1, py - 1, CYAN[1], CYAN[2], CYAN[3])
            end
            if screenX + 1 < WIDTH then setPixel(screenX + 1, py - 2, CYAN[1], CYAN[2], CYAN[3]) end
            if screenX + 2 < WIDTH then setPixel(screenX + 2, py - 1, CYAN[1], CYAN[2], CYAN[3]) end
        elseif frame == 1 then
            setPixel(screenX, py - 2, CYAN[1], CYAN[2], CYAN[3])
            if screenX - 1 >= 0 then setPixel(screenX - 1, py - 1, CYAN[1], CYAN[2], CYAN[3]) end
            if screenX + 1 < WIDTH then setPixel(screenX + 1, py - 1, CYAN[1], CYAN[2], CYAN[3]) end
        elseif frame == 2 then
            if screenX + 1 < WIDTH then
                setPixel(screenX + 1, py - 2, CYAN[1], CYAN[2], CYAN[3])
                setPixel(screenX + 1, py - 1, CYAN[1], CYAN[2], CYAN[3])
            end
            if screenX - 1 >= 0 then setPixel(screenX - 1, py - 2, CYAN[1], CYAN[2], CYAN[3]) end
            if screenX - 2 >= 0 then setPixel(screenX - 2, py - 1, CYAN[1], CYAN[2], CYAN[3]) end
        else
            setPixel(screenX, py - 2, CYAN[1], CYAN[2], CYAN[3])
            if screenX + 1 < WIDTH then setPixel(screenX + 1, py - 1, CYAN[1], CYAN[2], CYAN[3]) end
            if screenX - 1 >= 0 then setPixel(screenX - 1, py - 1, CYAN[1], CYAN[2], CYAN[3]) end
        end
    end

    -- Glow trail when running
    if not player.jumping then
        spawnParticles(player.x - 2, player.y, CYAN[1], CYAN[2], CYAN[3], 1)
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

local function drawScore()
    local scoreStr = string.format("%06d", score)
    local x = 4
    for i = 1, #scoreStr do
        local digit = tonumber(scoreStr:sub(i, i))
        drawDigit(digit, x, 2, MAGENTA[1], MAGENTA[2], MAGENTA[3])
        x = x + 5
    end
end

---------------------------------------------------------
-- Physics & AI
---------------------------------------------------------

local gravity = 150
local jumpStrength = -70

local function findNextPlatform()
    local bestPlatform = nil
    local bestScore = -math.huge

    for _, p in ipairs(platforms) do
        if p.x > player.x and p.x < player.x + 60 then
            local dist = p.x - player.x
            local heightDiff = math.abs(p.y - player.y)
            local sc = 100 - dist - heightDiff * 2

            if sc > bestScore then
                bestScore = sc
                bestPlatform = p
            end
        end
    end

    return bestPlatform
end

local function shouldJump()
    -- Check if there's a gap ahead
    local gapAhead = true
    for _, p in ipairs(platforms) do
        if player.x + 10 > p.x and player.x + 10 < p.x + p.width then
            if math.abs(player.y - p.y) < 10 then
                gapAhead = false
                break
            end
        end
    end

    -- Check for spikes ahead
    for _, s in ipairs(spikes) do
        if s.x > player.x and s.x < player.x + 15 then
            if math.abs(s.y - player.y) < 8 then
                return true
            end
        end
    end

    -- Jump to reach higher diamonds
    for _, d in ipairs(diamonds) do
        if not d.collected and d.x > player.x and d.x < player.x + 20 then
            if d.y < player.y - 5 then
                return true
            end
        end
    end

    -- Jump over gaps
    if gapAhead and player.onGround then
        local nextPlat = findNextPlatform()
        if nextPlat then
            return true
        end
    end

    return false
end

local function updatePlayer(dt)
    -- Running animation
    if player.onGround then
        player.runPhase = player.runPhase + dt * 12
    end

    -- Gravity
    player.vy = player.vy + gravity * dt

    -- AI Jump decision
    if player.onGround and shouldJump() then
        player.vy = jumpStrength
        player.jumping = true
        player.onGround = false
        spawnParticles(player.x, player.y, WHITE[1], WHITE[2], WHITE[3], 5)
    end

    -- Apply velocity
    player.y = player.y + player.vy * dt

    -- Platform collision
    player.onGround = false
    for _, p in ipairs(platforms) do
        if player.x + 2 > p.x and player.x - 2 < p.x + p.width then
            if player.y >= p.y - 1 and player.y <= p.y + 2 and player.vy >= 0 then
                player.y = p.y
                player.vy = 0
                player.onGround = true
                player.jumping = false
            end
        end
    end

    -- Collect diamonds
    for _, d in ipairs(diamonds) do
        if not d.collected then
            local dx = math.abs(player.x - d.x)
            local dy = math.abs(player.y - 4 - d.y)
            if dx < 5 and dy < 5 then
                d.collected = true
                score = score + 100
                spawnParticles(d.x, d.y, MAGENTA[1], MAGENTA[2], MAGENTA[3], 8)
            end
        end
    end

    -- Reset if fallen
    if player.y > HEIGHT + 20 then
        local safePlat = nil
        for _, p in ipairs(platforms) do
            if p.x > scrollX and p.x < scrollX + WIDTH then
                safePlat = p
                break
            end
        end
        if safePlat then
            player.x = safePlat.x + 10
            player.y = safePlat.y - 5
            player.vy = 0
        end
    end

    -- Keep player ahead of scroll
    if player.x < scrollX + 20 then
        player.x = scrollX + 20
    end
end

---------------------------------------------------------
-- Effect API
---------------------------------------------------------

function init(width, height)
    scrollX = 0
    scrollSpeed = 25

    player = {
        x = 25,
        y = 42,
        vy = 0,
        width = 4,
        height = 8,
        onGround = false,
        runPhase = 0,
        jumping = false
    }

    platforms = {}
    diamonds = {}
    spikes = {}
    particles = {}
    score = 0
    lastTime = 0

    initLevel()
end

function reset()
    init(WIDTH, HEIGHT)
end

function update(audio, settings, time)
    local dt = time - lastTime
    if dt <= 0 or dt > 0.1 then dt = 0.016 end
    lastTime = time

    clear()

    -- Scroll the level
    scrollSpeed = 30
    scrollX = scrollX + scrollSpeed * dt

    -- Move player with scroll (auto-run)
    player.x = player.x + scrollSpeed * dt

    -- Update game elements
    updatePlayer(dt)
    updateParticles(dt)
    cleanupAndGenerate()

    -- Draw everything
    drawBackground()
    drawPlatforms()
    drawSpikes()
    drawDiamonds()
    drawParticles()
    drawPlayer()
    drawScore()
end
