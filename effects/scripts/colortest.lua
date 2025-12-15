-- ====================================================================
-- COLOR TEST - Animated RGB gradients
-- 3 rows wiping at different speeds with random direction changes
-- ====================================================================

effect_name = "Color Test"
effect_description = "Animated RGB gradient wipe with random directions"

-- Animation speeds (pixels per second)
local redSpeed = 40
local greenSpeed = 60
local blueSpeed = 80

-- Direction: 1 = left-to-right, -1 = right-to-left
local redDir = 1
local greenDir = 1
local blueDir = 1

-- Track position for direction changes
local redPos = 0
local greenPos = 0
local bluePos = 0

local lastTime = 0
local initialized = false

function init(width, height)
    math.randomseed(os.time())
    redDir = math.random(2) == 1 and 1 or -1
    greenDir = math.random(2) == 1 and 1 or -1
    blueDir = math.random(2) == 1 and 1 or -1
    redPos = 0
    greenPos = 0
    bluePos = 0
    lastTime = 0
    initialized = true
end

function reset()
    redDir = math.random(2) == 1 and 1 or -1
    greenDir = math.random(2) == 1 and 1 or -1
    blueDir = math.random(2) == 1 and 1 or -1
    redPos = 0
    greenPos = 0
    bluePos = 0
    lastTime = 0
end

function update(audio, settings, time)
    if not initialized then
        init(WIDTH, HEIGHT)
    end

    local dt = time - lastTime
    if dt <= 0 or dt > 0.1 then dt = 0.016 end
    lastTime = time

    clear()

    local row_height = 21  -- 64 / 3 = ~21 pixels per row
    local cycleLength = WIDTH + 64

    -- Update positions
    redPos = redPos + redSpeed * dt * redDir
    greenPos = greenPos + greenSpeed * dt * greenDir
    bluePos = bluePos + blueSpeed * dt * blueDir

    -- Check for direction change (when reaching end of cycle)
    if redPos >= cycleLength or redPos <= -cycleLength then
        redDir = math.random(2) == 1 and 1 or -1
        redPos = 0
    end
    if greenPos >= cycleLength or greenPos <= -cycleLength then
        greenDir = math.random(2) == 1 and 1 or -1
        greenPos = 0
    end
    if bluePos >= cycleLength or bluePos <= -cycleLength then
        blueDir = math.random(2) == 1 and 1 or -1
        bluePos = 0
    end

    -- Row 1: Red gradient
    for x = 0, WIDTH - 1 do
        local drawX = x
        if redDir < 0 then drawX = WIDTH - 1 - x end

        local dist = drawX - (math.abs(redPos) - 64)
        if dist >= 0 and dist < WIDTH then
            local r = math.floor(dist * 255 / (WIDTH - 1))
            r = math.min(255, math.max(0, r))
            for y = 0, row_height - 1 do
                setPixel(x, y, r, 0, 0)
            end
        end
    end

    -- Row 2: Green gradient
    for x = 0, WIDTH - 1 do
        local drawX = x
        if greenDir < 0 then drawX = WIDTH - 1 - x end

        local dist = drawX - (math.abs(greenPos) - 64)
        if dist >= 0 and dist < WIDTH then
            local g = math.floor(dist * 255 / (WIDTH - 1))
            g = math.min(255, math.max(0, g))
            for y = row_height, row_height * 2 - 1 do
                setPixel(x, y, 0, g, 0)
            end
        end
    end

    -- Row 3: Blue gradient
    for x = 0, WIDTH - 1 do
        local drawX = x
        if blueDir < 0 then drawX = WIDTH - 1 - x end

        local dist = drawX - (math.abs(bluePos) - 64)
        if dist >= 0 and dist < WIDTH then
            local b = math.floor(dist * 255 / (WIDTH - 1))
            b = math.min(255, math.max(0, b))
            for y = row_height * 2, HEIGHT - 1 do
                setPixel(x, y, 0, 0, b)
            end
        end
    end
end
