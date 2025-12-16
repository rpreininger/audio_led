-- ====================================================================
-- GEISS - Inspired by the classic Geiss Winamp plugin
-- Fluid flowing patterns with spectrum reaction
-- ====================================================================

effect_name = "Geiss (Lua)"
effect_description = "Flowing fluid patterns like Geiss plugin"

local time = 0
local hue = 0
local flowX = 0
local flowY = 0

function init(width, height)
    time = 0
    hue = 0
end

function reset()
    init(WIDTH, HEIGHT)
end

function update(audio, settings, time_param)
    local vol = audio.volume or 0
    local beat = audio.beat and 1 or 0
    local bass = audio.bass or 0
    local mid = audio.mid or 0

    if vol < settings.noiseThreshold then vol = 0 end

    time = time + 0.03 + vol * 0.05
    hue = hue + 0.3 + beat * 2
    if hue >= 360 then hue = hue - 360 end

    -- Flow direction changes with audio
    flowX = flowX + (bass - 0.5) * 0.1
    flowY = flowY + (mid - 0.5) * 0.1

    local cx = WIDTH / 2
    local cy = HEIGHT / 2

    for y = 0, HEIGHT - 1 do
        for x = 0, WIDTH - 1 do
            -- Flowing coordinates
            local fx = x + math.sin(y * 0.1 + time) * 5 * vol + flowX
            local fy = y + math.cos(x * 0.1 + time * 0.8) * 5 * vol + flowY

            -- Multiple sine waves for fluid look
            local v1 = math.sin(fx * 0.05 + time)
            local v2 = math.sin(fy * 0.05 + time * 1.2)
            local v3 = math.sin((fx + fy) * 0.03 + time * 0.7)
            local v4 = math.sin(math.sqrt((fx - cx)^2 + (fy - cy)^2) * 0.08 - time)

            local v = (v1 + v2 + v3 + v4) / 4

            -- Add spectrum influence
            local specIndex = math.floor(x / WIDTH * 7) + 1
            local specVal = audio.spectrum[specIndex] or 0
            v = v + specVal * 0.3

            -- Color mapping
            local h = (hue + v * 60 + x * 0.5) % 360
            local bright = 0.2 + (v + 1) * 0.3 + vol * 0.2

            -- Distance fade from center
            local dx = x - cx
            local dy = y - cy
            local dist = math.sqrt(dx * dx + dy * dy)
            local fade = 1 - dist / (WIDTH * 0.7)
            if fade < 0.2 then fade = 0.2 end
            bright = bright * fade

            setPixelHSV(x, y, h, 0.8, math.max(0, math.min(1, bright)))
        end
    end

    -- Spectrum bars at bottom (subtle)
    for i = 1, 8 do
        local specVal = audio.spectrum[i] or 0
        local barX = (i - 1) * (WIDTH / 8)
        local barW = WIDTH / 8 - 2
        local barH = math.floor(specVal * 15)

        for bx = barX, barX + barW do
            for by = HEIGHT - barH, HEIGHT - 1 do
                if bx >= 0 and bx < WIDTH and by >= 0 and by < HEIGHT then
                    local h = (hue + i * 45) % 360
                    setPixelHSV(math.floor(bx), math.floor(by), h, 1, 0.6)
                end
            end
        end
    end
end
