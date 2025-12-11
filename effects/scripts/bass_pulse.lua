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

-- Called once when effect is loaded
function init(width, height)
    hue = 0
end

-- Called when effect is reset
function reset()
    hue = 0
end

-- Called every frame
-- audio: table with volume, beat, bass, mid, treble, spectrum[1-8]
-- settings: table with brightness, sensitivity, noiseThreshold
-- time: elapsed time in seconds
function update(audio, settings, time)
    -- Clear screen
    clear()

    -- Update hue over time
    hue = hue + hue_speed
    if hue >= 360 then hue = hue - 360 end

    -- Calculate radius from bass
    local bass = audio.bass or 0
    if bass < settings.noiseThreshold then bass = 0 end

    local radius = bass * radius_scale
    if radius < 3 then radius = 3 end
    if radius > 30 then radius = 30 end

    -- Draw filled circle in center
    local cx = WIDTH / 2
    local cy = HEIGHT / 2

    fillCircle(cx, cy, radius, 1, 0.5, 0.5)

    -- Draw outer ring with HSV color
    local ringRadius = radius + 3 + audio.beat * 5
    for angle = 0, 360, 5 do
        local rad = math.rad(angle)
        local x = cx + math.cos(rad) * ringRadius
        local y = cy + math.sin(rad) * ringRadius
        setPixelHSV(math.floor(x), math.floor(y), (hue + angle) % 360, 1, 1)
    end
end
