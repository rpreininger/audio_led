-- ====================================================================
-- COLOR TEST - Animated RGB gradients
-- 3 rows wiping from left to right at different speeds
-- ====================================================================

effect_name = "Color Test"
effect_description = "Animated RGB gradient wipe test"

-- Animation speeds (pixels per second)
local redSpeed = 40
local greenSpeed = 60
local blueSpeed = 80

function init(width, height)
end

function reset()
end

function update(audio, settings, time)
    clear()

    local row_height = 21  -- 64 / 3 = ~21 pixels per row

    -- Calculate wipe positions (loop back after completing)
    local redPos = math.floor((time * redSpeed) % (WIDTH + 64))
    local greenPos = math.floor((time * greenSpeed) % (WIDTH + 64))
    local bluePos = math.floor((time * blueSpeed) % (WIDTH + 64))

    -- Row 1: Red gradient wiping left to right
    for x = 0, WIDTH - 1 do
        local dist = x - (redPos - 64)
        if dist >= 0 and dist < WIDTH then
            local r = math.floor(dist * 255 / (WIDTH - 1))
            r = math.min(255, math.max(0, r))
            for y = 0, row_height - 1 do
                setPixel(x, y, r, 0, 0)
            end
        end
    end

    -- Row 2: Green gradient wiping left to right
    for x = 0, WIDTH - 1 do
        local dist = x - (greenPos - 64)
        if dist >= 0 and dist < WIDTH then
            local g = math.floor(dist * 255 / (WIDTH - 1))
            g = math.min(255, math.max(0, g))
            for y = row_height, row_height * 2 - 1 do
                setPixel(x, y, 0, g, 0)
            end
        end
    end

    -- Row 3: Blue gradient wiping left to right
    for x = 0, WIDTH - 1 do
        local dist = x - (bluePos - 64)
        if dist >= 0 and dist < WIDTH then
            local b = math.floor(dist * 255 / (WIDTH - 1))
            b = math.min(255, math.max(0, b))
            for y = row_height * 2, HEIGHT - 1 do
                setPixel(x, y, 0, 0, b)
            end
        end
    end
end
