-- ====================================================================
-- BASS PULSE EFFECT - Lua scripted effect
-- A pulsing circle that responds to bass frequencies
-- ====================================================================

effect_name = "Bass Pulse (Lua)"
effect_description = "Pulsing circle based on bass level"

-- Local state
local hue = 0
local hue_speed = 2
local radius_scale = 30
local rotation = 0
local rotation2 = 0

-- Called once when effect is loaded
function init(width, height)
    hue = 0
    rotation = 0
    rotation2 = 0
end

-- Called when effect is reset
function reset()
    hue = 0
    rotation = 0
    rotation2 = 0
end

-- Called every frame
-- audio: table with volume, beat, bass, mid, treble, spectrum[1-8]
-- settings: table with brightness, sensitivity, noiseThreshold
-- time: elapsed time in seconds
function update(audio, settings, time)
    -- Clear screen
    clear()

    local beat = audio.beat and 1 or 0

    -- Update hue over time
    hue = hue + hue_speed + beat * 5
    if hue >= 360 then hue = hue - 360 end

    -- Update rotation (faster on beat)
    rotation = rotation + 2 + beat * 8
    rotation2 = rotation2 - 1.5 - beat * 6  -- Counter-rotate
    if rotation >= 360 then rotation = rotation - 360 end
    if rotation2 < 0 then rotation2 = rotation2 + 360 end

    -- Calculate radius from bass
    local bass = audio.bass or 0
    if bass < settings.noiseThreshold then bass = 0 end

    local radius = bass * radius_scale
    if radius < 5 then radius = 5 end
    if radius > 25 then radius = 25 end

    -- Draw filled circle in center
    local cx = WIDTH / 2
    local cy = HEIGHT / 2

    -- Manual filled circle with gradient
    for dy = -radius, radius do
        for dx = -radius, radius do
            local dist = math.sqrt(dx * dx + dy * dy)
            if dist <= radius then
                local x = math.floor(cx + dx)
                local y = math.floor(cy + dy)
                if x >= 0 and x < WIDTH and y >= 0 and y < HEIGHT then
                    local brightness = 0.3 + 0.4 * (1 - dist / radius)
                    setPixelHSV(x, y, hue, 1, brightness)
                end
            end
        end
    end

    -- Inner rotating ring
    local ringRadius1 = radius + 4 + beat * 3
    local numDots1 = 12
    for i = 0, numDots1 - 1 do
        local angle = rotation + i * (360 / numDots1)
        local rad = math.rad(angle)
        local x = cx + math.cos(rad) * ringRadius1
        local y = cy + math.sin(rad) * ringRadius1
        if x >= 0 and x < WIDTH and y >= 0 and y < HEIGHT then
            setPixelHSV(math.floor(x), math.floor(y), (hue + angle) % 360, 1, 1)
        end
    end

    -- Outer rotating ring (counter-rotating)
    local ringRadius2 = radius + 10 + beat * 5
    local numDots2 = 16
    for i = 0, numDots2 - 1 do
        local angle = rotation2 + i * (360 / numDots2)
        local rad = math.rad(angle)
        local x = cx + math.cos(rad) * ringRadius2
        local y = cy + math.sin(rad) * ringRadius2
        if x >= 0 and x < WIDTH and y >= 0 and y < HEIGHT then
            setPixelHSV(math.floor(x), math.floor(y), (hue + 180 + angle) % 360, 0.8, 0.9)
        end
    end

    -- Outermost ring - orbiting particles
    local ringRadius3 = radius + 18 + beat * 4
    local numDots3 = 8
    for i = 0, numDots3 - 1 do
        local angle = rotation * 0.5 + i * (360 / numDots3)
        local rad = math.rad(angle)
        -- Add wobble
        local wobble = math.sin(time * 3 + i) * 3
        local r = ringRadius3 + wobble
        local x = cx + math.cos(rad) * r
        local y = cy + math.sin(rad) * r
        if x >= 0 and x < WIDTH and y >= 0 and y < HEIGHT then
            setPixelHSV(math.floor(x), math.floor(y), (hue + 90 + angle * 2) % 360, 0.6, 1)
        end
    end
end
