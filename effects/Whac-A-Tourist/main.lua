---------------------------------------------------------
-- Whac-A-Tourist - 128x64
-- Love2D Whac-A-Mole style game
-- Hit the tourists with the Spanish mallet!
-- SELF-RUNNING DEMO MODE with AI Player
---------------------------------------------------------

-- Virtual display
WIDTH  = 128
HEIGHT = 64
PIXEL_SIZE = 8

-- Images
local images = {}

-- Game state
local gameState = "menu"  -- "menu", "playing", "gameover"
local score = 0
local timeLeft = 60
local combo = 0
local comboTimer = 0
local menuSelection = 1   -- 1 = Play, 2 = Demo

-- Demo mode (AI controlled)
local demoMode = false
local ai = {
    targetHole = nil,
    reactionDelay = 0,      -- Simulates human reaction time
    missChance = 0.15,      -- Sometimes misses on purpose
    moveSpeed = 120,        -- Pixels per second
    waitTimer = 0,
    state = "idle"          -- idle, moving, hitting
}

-- Mallet / Spanish character
local mallet = {
    x = 64,
    y = 32,
    frame = 1,         -- 1 = raised, 2 = hitting
    hitTimer = 0,
    hitDuration = 0.15
}

-- Holes where tourists pop up (positions for 3x2 grid)
local holes = {}
local holePositions = {
    {x = 20, y = 30},
    {x = 64, y = 30},
    {x = 108, y = 30},
    {x = 20, y = 52},
    {x = 64, y = 52},
    {x = 108, y = 52}
}

-- Tourist spawn settings
local spawnTimer = 0
local spawnInterval = 1.5
local minSpawnInterval = 0.4
local difficultyTimer = 0

-- Particles for hit effects
local particles = {}

---------------------------------------------------------
-- Helper Functions
---------------------------------------------------------

local function spawnParticles(x, y, count)
    for i = 1, count do
        table.insert(particles, {
            x = x,
            y = y,
            vx = (math.random() - 0.5) * 80,
            vy = (math.random() - 0.5) * 80 - 30,
            life = 0.5 + math.random() * 0.3,
            size = 2 + math.random() * 2
        })
    end
end

local function updateParticles(dt)
    for i = #particles, 1, -1 do
        local p = particles[i]
        p.x = p.x + p.vx * dt
        p.y = p.y + p.vy * dt
        p.vy = p.vy + 150 * dt
        p.life = p.life - dt
        p.size = p.size * 0.95
        if p.life <= 0 then
            table.remove(particles, i)
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
            popupProgress = 0,   -- 0 = hidden, 1 = fully up
            state = "hidden",    -- hidden, rising, visible, falling, hit
            stateTimer = 0,
            visibleDuration = 1.5,
            wasHit = false
        })
    end
end

local function spawnTouristInRandomHole()
    -- Find empty holes
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
        -- Vary visible duration based on difficulty
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
                -- Missed! Reset combo
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
            -- Quick fall after being hit
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

    -- Check if mallet is over any visible tourist
    for _, hole in ipairs(holes) do
        if (hole.state == "visible" or hole.state == "rising") and hole.popupProgress > 0.5 then
            local dx = math.abs(mallet.x - hole.x)
            local dy = math.abs(mallet.y - (hole.y - 10 * hole.popupProgress))

            if dx < 20 and dy < 20 then
                -- Hit!
                hole.state = "hit"
                hole.wasHit = true
                combo = combo + 1
                comboTimer = 2

                -- Score with combo bonus
                local points = 100 * combo
                score = score + points

                -- Particles!
                spawnParticles(hole.x, hole.y - 8, 10)

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
            -- Prioritize tourists that have been visible longer (about to disappear)
            local urgency = hole.stateTimer
            -- Also consider distance (prefer closer targets)
            local dx = math.abs(mallet.x - hole.x)
            local dy = math.abs(mallet.y - (hole.y - 10))
            local distance = math.sqrt(dx * dx + dy * dy)

            -- Score: higher urgency and lower distance is better
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
    if not demoMode then return end

    -- Handle reaction delay (simulates human reaction time)
    if ai.reactionDelay > 0 then
        ai.reactionDelay = ai.reactionDelay - dt
        return
    end

    -- Handle wait timer (pause between actions)
    if ai.waitTimer > 0 then
        ai.waitTimer = ai.waitTimer - dt
        return
    end

    -- Don't do anything while hitting
    if mallet.hitTimer > 0 then
        return
    end

    -- Find a target if we don't have one
    if ai.targetHole == nil or ai.targetHole.state == "hidden" or ai.targetHole.state == "hit" then
        ai.targetHole = findBestTarget()
        if ai.targetHole then
            -- Add small reaction delay when spotting new target
            ai.reactionDelay = 0.05 + math.random() * 0.15
            ai.state = "moving"
        else
            ai.state = "idle"
            -- Wander randomly when no targets
            if math.random() < 0.02 then
                mallet.x = mallet.x + (math.random() - 0.5) * 10
                mallet.y = mallet.y + (math.random() - 0.5) * 10
            end
        end
        return
    end

    -- Move towards target
    local targetX = ai.targetHole.x
    local targetY = ai.targetHole.y - 8  -- Aim slightly above hole

    -- Add slight randomness to target (imperfect aim)
    if math.random() < 0.1 then
        targetX = targetX + (math.random() - 0.5) * 6
        targetY = targetY + (math.random() - 0.5) * 6
    end

    local dx = targetX - mallet.x
    local dy = targetY - mallet.y
    local distance = math.sqrt(dx * dx + dy * dy)

    if distance > 3 then
        -- Move towards target
        local moveAmount = ai.moveSpeed * dt
        if moveAmount > distance then
            moveAmount = distance
        end

        mallet.x = mallet.x + (dx / distance) * moveAmount
        mallet.y = mallet.y + (dy / distance) * moveAmount
        ai.state = "moving"
    else
        -- Close enough - try to hit!
        ai.state = "hitting"

        -- Chance to miss (makes it more realistic)
        if math.random() < ai.missChance then
            -- Miss! Wait a bit then try again
            ai.waitTimer = 0.1 + math.random() * 0.2
            ai.targetHole = nil
        else
            tryHitTourist()
            ai.targetHole = nil
            ai.waitTimer = 0.1 + math.random() * 0.15
        end
    end

    -- Keep mallet in bounds
    mallet.x = math.max(10, math.min(WIDTH - 10, mallet.x))
    mallet.y = math.max(10, math.min(HEIGHT - 5, mallet.y))
end

---------------------------------------------------------
-- Drawing
---------------------------------------------------------

local function drawBackground()
    -- Scale and draw background to fill screen
    local bg = images.background
    if bg then
        local scaleX = (WIDTH * PIXEL_SIZE) / bg:getWidth()
        local scaleY = (HEIGHT * PIXEL_SIZE) / bg:getHeight()
        love.graphics.setColor(1, 1, 1)
        love.graphics.draw(bg, 0, 0, 0, scaleX, scaleY)
    end
end

local function drawHoles()
    for _, hole in ipairs(holes) do
        -- Draw hole (dark ellipse)
        love.graphics.setColor(0.2, 0.15, 0.1)
        local holeScreenX = hole.x * PIXEL_SIZE
        local holeScreenY = hole.y * PIXEL_SIZE
        love.graphics.ellipse("fill", holeScreenX, holeScreenY, 24 * PIXEL_SIZE / 8, 10 * PIXEL_SIZE / 8)

        -- Draw tourist if present
        if hole.hasTourist and hole.popupProgress > 0 then
            local touristImg = images.tourists[hole.touristType]
            if touristImg then
                local imgW = touristImg:getWidth()
                local imgH = touristImg:getHeight()
                local scale = 3.0  -- Bigger tourists!

                -- Calculate position (tourist rises from hole)
                local riseAmount = hole.popupProgress * imgH * scale * 0.85
                local touristX = holeScreenX - (imgW * scale / 2)
                local touristY = holeScreenY - riseAmount

                -- Use stencil to clip tourist below hole
                love.graphics.stencil(function()
                    love.graphics.rectangle("fill", 0, 0, WIDTH * PIXEL_SIZE, holeScreenY)
                end, "replace", 1)
                love.graphics.setStencilTest("greater", 0)

                -- Flash red if just hit
                if hole.wasHit then
                    love.graphics.setColor(1, 0.3, 0.3)
                else
                    love.graphics.setColor(1, 1, 1)
                end

                love.graphics.draw(touristImg, touristX, touristY, 0, scale, scale)

                love.graphics.setStencilTest()
            end
        end
    end
end

local function drawMallet()
    local malletImg = mallet.frame == 1 and images.mallet1 or images.mallet2
    if malletImg then
        local imgW = malletImg:getWidth()
        local imgH = malletImg:getHeight()
        local scale = 2

        -- Position mallet so right edge is at cursor
        local mx = mallet.x * PIXEL_SIZE - imgW * scale
        local my = mallet.y * PIXEL_SIZE - imgH * scale / 2

        love.graphics.setColor(1, 1, 1)
        love.graphics.draw(malletImg, mx, my, 0, scale, scale)
    end
end

local function drawParticles()
    love.graphics.setColor(1, 0.9, 0.2)
    for _, p in ipairs(particles) do
        local alpha = p.life / 0.8
        love.graphics.setColor(1, 0.9, 0.2, alpha)
        love.graphics.circle("fill", p.x * PIXEL_SIZE / 8, p.y * PIXEL_SIZE / 8, p.size * PIXEL_SIZE / 8)
    end
end

local function drawHUD()
    -- Score (dark green)
    love.graphics.setColor(0, 0.5, 0)
    love.graphics.setFont(love.graphics.newFont(24))
    love.graphics.print("Score: " .. score, 10, 10)

    -- Time (dark green, red when low)
    local timeColor = timeLeft < 10 and {1, 0.3, 0.3} or {0, 0.5, 0}
    love.graphics.setColor(timeColor[1], timeColor[2], timeColor[3])
    love.graphics.print("Time: " .. math.ceil(timeLeft), WIDTH * PIXEL_SIZE - 150, 10)

    -- Combo
    if combo > 1 and comboTimer > 0 then
        love.graphics.setColor(1, 1, 0)
        love.graphics.print("COMBO x" .. combo .. "!", WIDTH * PIXEL_SIZE / 2 - 60, 50)
    end

    -- Demo mode indicator
    if demoMode then
        love.graphics.setColor(1, 1, 0, 0.8)
        love.graphics.setFont(love.graphics.newFont(18))
        love.graphics.print("DEMO", WIDTH * PIXEL_SIZE / 2 - 25, HEIGHT * PIXEL_SIZE - 30)
    end
end

local restartTimer = 0

local function drawMenu()
    -- Draw background
    drawBackground()

    -- Darken for menu overlay
    love.graphics.setColor(0, 0, 0, 0.6)
    love.graphics.rectangle("fill", 0, 0, WIDTH * PIXEL_SIZE, HEIGHT * PIXEL_SIZE)

    -- Title
    love.graphics.setColor(1, 0.9, 0.2)
    love.graphics.setFont(love.graphics.newFont(48))
    love.graphics.printf("WHAC-A-TOURIST!", 0, HEIGHT * PIXEL_SIZE / 2 - 120, WIDTH * PIXEL_SIZE, "center")

    -- Menu options
    love.graphics.setFont(love.graphics.newFont(32))

    -- Play option
    if menuSelection == 1 then
        love.graphics.setColor(1, 1, 0)
        love.graphics.printf("> PLAY <", 0, HEIGHT * PIXEL_SIZE / 2 - 20, WIDTH * PIXEL_SIZE, "center")
    else
        love.graphics.setColor(0.7, 0.7, 0.7)
        love.graphics.printf("PLAY", 0, HEIGHT * PIXEL_SIZE / 2 - 20, WIDTH * PIXEL_SIZE, "center")
    end

    -- Demo option
    if menuSelection == 2 then
        love.graphics.setColor(1, 1, 0)
        love.graphics.printf("> DEMO <", 0, HEIGHT * PIXEL_SIZE / 2 + 30, WIDTH * PIXEL_SIZE, "center")
    else
        love.graphics.setColor(0.7, 0.7, 0.7)
        love.graphics.printf("DEMO", 0, HEIGHT * PIXEL_SIZE / 2 + 30, WIDTH * PIXEL_SIZE, "center")
    end

    -- Instructions
    love.graphics.setColor(0.8, 0.8, 0.8)
    love.graphics.setFont(love.graphics.newFont(18))
    love.graphics.printf("UP/DOWN to select, ENTER to start", 0, HEIGHT * PIXEL_SIZE - 50, WIDTH * PIXEL_SIZE, "center")
end

local function drawGameOver()
    -- Darken background
    love.graphics.setColor(0, 0, 0, 0.7)
    love.graphics.rectangle("fill", 0, 0, WIDTH * PIXEL_SIZE, HEIGHT * PIXEL_SIZE)

    -- Game over text
    love.graphics.setColor(1, 1, 1)
    love.graphics.setFont(love.graphics.newFont(48))
    love.graphics.printf("GAME OVER!", 0, HEIGHT * PIXEL_SIZE / 2 - 60, WIDTH * PIXEL_SIZE, "center")

    love.graphics.setFont(love.graphics.newFont(32))
    love.graphics.printf("Final Score: " .. score, 0, HEIGHT * PIXEL_SIZE / 2, WIDTH * PIXEL_SIZE, "center")

    if demoMode then
        love.graphics.setFont(love.graphics.newFont(20))
        love.graphics.setColor(1, 1, 0)
        love.graphics.printf("Restarting in " .. math.ceil(restartTimer) .. "...", 0, HEIGHT * PIXEL_SIZE / 2 + 60, WIDTH * PIXEL_SIZE, "center")
    else
        love.graphics.setFont(love.graphics.newFont(20))
        love.graphics.printf("Press SPACE or Click to restart", 0, HEIGHT * PIXEL_SIZE / 2 + 60, WIDTH * PIXEL_SIZE, "center")
    end
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

    -- Reset AI state
    ai.targetHole = nil
    ai.reactionDelay = 0
    ai.waitTimer = 0
    ai.state = "idle"
    ai.moveSpeed = 100 + math.random() * 40  -- Vary AI speed each round

    -- Reset mallet position
    mallet.x = 64
    mallet.y = 32
    mallet.frame = 1
    mallet.hitTimer = 0

    initHoles()
end

local function updateGame(dt)
    if gameState == "playing" then
        -- Update time
        timeLeft = timeLeft - dt
        if timeLeft <= 0 then
            timeLeft = 0
            gameState = "gameover"
            restartTimer = 3
            return
        end

        -- Increase difficulty over time
        difficultyTimer = difficultyTimer + dt
        if difficultyTimer > 5 then
            difficultyTimer = 0
            spawnInterval = math.max(minSpawnInterval, spawnInterval - 0.1)
        end

        -- Spawn tourists
        spawnTimer = spawnTimer + dt
        if spawnTimer >= spawnInterval then
            spawnTimer = 0
            spawnTouristInRandomHole()
            -- Sometimes spawn two at once when difficulty is high
            if spawnInterval < 0.8 and math.random() < 0.3 then
                spawnTouristInRandomHole()
            end
        end

        -- Update combo timer
        if comboTimer > 0 then
            comboTimer = comboTimer - dt
            if comboTimer <= 0 then
                combo = 0
            end
        end

        -- Update AI in demo mode
        updateAI(dt)

        -- Update game objects
        updateHoles(dt)
        updateMallet(dt)
        updateParticles(dt)
    elseif gameState == "gameover" then
        -- Auto-restart in demo mode
        if demoMode then
            restartTimer = restartTimer - dt
            if restartTimer <= 0 then
                resetGame()
            end
        end
    end
end

---------------------------------------------------------
-- Love2D Callbacks
---------------------------------------------------------

function love.load()
    love.window.setMode(WIDTH * PIXEL_SIZE, HEIGHT * PIXEL_SIZE)
    love.window.setTitle("Whac-A-Tourist!")
    love.mouse.setVisible(true)

    math.randomseed(os.time())

    -- Load images
    local imagePath = "images/"

    -- Background
    local bgSuccess, bg = pcall(love.graphics.newImage, imagePath .. "backgound/island.png")
    if bgSuccess then
        images.background = bg
    end

    -- Mallet sprites
    local m1Success, m1 = pcall(love.graphics.newImage, imagePath .. "spain/mallet1.png")
    if m1Success then
        images.mallet1 = m1
    end

    local m2Success, m2 = pcall(love.graphics.newImage, imagePath .. "spain/mallet2.png")
    if m2Success then
        images.mallet2 = m2
    end

    -- Tourists
    images.tourists = {}
    for i = 1, 3 do
        local tSuccess, t = pcall(love.graphics.newImage, imagePath .. "tourist/tourist" .. i .. ".png")
        if tSuccess then
            images.tourists[i] = t
        end
    end

    initHoles()
end

function love.update(dt)
    if gameState == "menu" then
        return
    end

    -- Update mallet position from mouse (only in manual mode)
    if not demoMode then
        local mx, my = love.mouse.getPosition()
        mallet.x = mx / PIXEL_SIZE
        mallet.y = my / PIXEL_SIZE
    end

    updateGame(dt)
end

function love.draw()
    if gameState == "menu" then
        drawMenu()
    else
        -- Draw game
        drawBackground()
        drawHoles()
        drawParticles()
        drawMallet()
        drawHUD()

        if gameState == "gameover" then
            drawGameOver()
        end
    end
end

local function startGame(demo)
    demoMode = demo
    love.mouse.setVisible(demoMode)
    love.window.setTitle(demoMode and "Whac-A-Tourist! - Demo" or "Whac-A-Tourist!")
    resetGame()
    gameState = "playing"
end

function love.mousepressed(x, y, button)
    if button == 1 then
        if gameState == "menu" then
            -- Check if clicked on menu options
            local centerY = HEIGHT * PIXEL_SIZE / 2
            if y >= centerY - 40 and y <= centerY + 10 then
                startGame(false)  -- Play
            elseif y >= centerY + 10 and y <= centerY + 60 then
                startGame(true)   -- Demo
            end
        elseif gameState == "playing" then
            tryHitTourist()
        elseif gameState == "gameover" then
            if demoMode then
                resetGame()
                gameState = "playing"
            else
                gameState = "menu"
            end
        end
    end
end

function love.keypressed(key)
    if key == "escape" then
        if gameState == "menu" then
            love.event.quit()
        else
            -- Return to menu
            gameState = "menu"
            love.mouse.setVisible(true)
        end
    elseif gameState == "menu" then
        -- Menu navigation
        if key == "up" or key == "w" then
            menuSelection = menuSelection - 1
            if menuSelection < 1 then menuSelection = 2 end
        elseif key == "down" or key == "s" then
            menuSelection = menuSelection + 1
            if menuSelection > 2 then menuSelection = 1 end
        elseif key == "return" or key == "space" then
            startGame(menuSelection == 2)
        end
    elseif gameState == "playing" then
        if key == "space" then
            tryHitTourist()
        end
    elseif gameState == "gameover" then
        if key == "space" or key == "return" then
            if demoMode then
                resetGame()
                gameState = "playing"
            else
                gameState = "menu"
            end
        end
    end
end
