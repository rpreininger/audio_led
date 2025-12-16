-- ====================================================================
-- MILKDROP - Inspired by Winamp's Milkdrop visualizer
-- Morphing geometric patterns with motion
-- ====================================================================

effect_name = "Milkdrop (Lua)"
effect_description = "Morphing patterns inspired by Milkdrop"

local time = 0
local hue = 0
local mode = 0
local modeTimer = 0
local zoom = 1.0
local warpX = 0
local warpY = 0

function init(width, height)
    time = 0
    hue = 0
    mode = 0
    modeTimer = 0
end

function reset()
    init(WIDTH, HEIGHT)
end

-- Plasma function
local function plasma(x, y, t)
    local v = math.sin(x * 0.1 + t)
    v = v + math.sin((y * 0.1 + t) * 0.5)
    v = v + math.sin((x * 0.1 + y * 0.1 + t) * 0.5)
    local cx = x + math.sin(t * 0.3) * 20
    local cy = y + math.cos(t * 0.5) * 20
    v = v + math.sin(math.sqrt(cx * cx + cy * cy) * 0.1)
    return v / 4
end

function update(audio, settings, time_param)
    local vol = audio.volume or 0
    local beat = audio.beat and 1 or 0
    local bass = audio.bass or 0
    local mid = audio.mid or 0
    local treble = audio.treble or 0

    if vol < settings.noiseThreshold then vol = 0 end

    -- Time progression
    time = time + 0.05 + vol * 0.1
    hue = hue + 0.5 + beat * 3
    if hue >= 360 then hue = hue - 360 end

    -- Mode switching
    modeTimer = modeTimer + 0.016
    if modeTimer > 8 then
        mode = (mode + 1) % 4
        modeTimer = 0
    end

    -- Zoom pulses with beat
    zoom = 1.0 + beat * 0.3 + bass * 0.2

    -- Warp movement
    warpX = math.sin(time * 0.3) * 10 * vol
    warpY = math.cos(time * 0.4) * 10 * vol

    local cx = WIDTH / 2
    local cy = HEIGHT / 2

    for y = 0, HEIGHT - 1 do
        for x = 0, WIDTH - 1 do
            -- Centered coordinates with zoom
            local dx = (x - cx) / zoom + warpX
            local dy = (y - cy) / zoom + warpY

            local r, g, b = 0, 0, 0

            if mode == 0 then
                -- Plasma mode
                local v = plasma(dx, dy, time)
                local h = (hue + v * 180) % 360
                local s = 0.8 + v * 0.2
                local bright = 0.3 + vol * 0.4 + math.abs(v) * 0.3

                -- HSV to RGB
                local hi = math.floor(h / 60) % 6
                local f = h / 60 - math.floor(h / 60)
                local p = bright * (1 - s)
                local q = bright * (1 - f * s)
                local t = bright * (1 - (1 - f) * s)

                if hi == 0 then r, g, b = bright, t, p
                elseif hi == 1 then r, g, b = q, bright, p
                elseif hi == 2 then r, g, b = p, bright, t
                elseif hi == 3 then r, g, b = p, q, bright
                elseif hi == 4 then r, g, b = t, p, bright
                else r, g, b = bright, p, q end

            elseif mode == 1 then
                -- Radial waves
                local dist = math.sqrt(dx * dx + dy * dy)
                local angle = math.atan(dy, dx)
                local wave = math.sin(dist * 0.3 - time * 2 + bass * 5)
                wave = wave + math.sin(angle * 4 + time + mid * 3) * 0.5

                local h = (hue + dist * 3 + wave * 30) % 360
                local bright = 0.2 + (wave + 1) * 0.3 + vol * 0.3

                local hi = math.floor(h / 60) % 6
                local f = h / 60 - math.floor(h / 60)
                if hi == 0 then r, g, b = bright, bright * f, 0
                elseif hi == 1 then r, g, b = bright * (1-f), bright, 0
                elseif hi == 2 then r, g, b = 0, bright, bright * f
                elseif hi == 3 then r, g, b = 0, bright * (1-f), bright
                elseif hi == 4 then r, g, b = bright * f, 0, bright
                else r, g, b = bright, 0, bright * (1-f) end

            elseif mode == 2 then
                -- Tunnel effect
                local dist = math.sqrt(dx * dx + dy * dy) + 0.1
                local angle = math.atan(dy, dx)

                local u = 1 / dist + time * 0.5
                local v = angle / math.pi

                local pattern = math.sin(u * 10) * math.sin(v * 10 + treble * 5)
                local h = (hue + u * 100) % 360
                local bright = 0.2 + (pattern + 1) * 0.25 + vol * 0.3
                bright = bright * (1 - math.min(dist / 50, 0.8))

                local hi = math.floor(h / 60) % 6
                local f = h / 60 - math.floor(h / 60)
                if hi == 0 then r, g, b = bright, bright * f * 0.8, 0
                elseif hi == 1 then r, g, b = bright * (1-f), bright, 0
                elseif hi == 2 then r, g, b = 0, bright, bright * f
                elseif hi == 3 then r, g, b = 0, bright * (1-f), bright
                elseif hi == 4 then r, g, b = bright * f, 0, bright
                else r, g, b = bright, 0, bright * (1-f) end

            else
                -- Kaleidoscope
                local angle = math.atan(dy, dx)
                local dist = math.sqrt(dx * dx + dy * dy)

                -- Mirror angle into segments
                local segments = 6
                local segAngle = math.abs(((angle + math.pi) % (2 * math.pi / segments)) - math.pi / segments)

                local kx = math.cos(segAngle) * dist
                local ky = math.sin(segAngle) * dist

                local pattern = math.sin(kx * 0.2 + time) + math.sin(ky * 0.2 - time * 0.7)
                pattern = pattern + math.sin(dist * 0.15 - time * bass * 2)

                local h = (hue + pattern * 60 + dist) % 360
                local bright = 0.2 + (pattern + 2) * 0.2 + vol * 0.3

                local hi = math.floor(h / 60) % 6
                local f = h / 60 - math.floor(h / 60)
                if hi == 0 then r, g, b = bright, bright * f, 0
                elseif hi == 1 then r, g, b = bright * (1-f), bright, 0
                elseif hi == 2 then r, g, b = 0, bright, bright * f
                elseif hi == 3 then r, g, b = 0, bright * (1-f), bright
                elseif hi == 4 then r, g, b = bright * f, 0, bright
                else r, g, b = bright, 0, bright * (1-f) end
            end

            -- Clamp and set pixel
            r = math.max(0, math.min(1, r))
            g = math.max(0, math.min(1, g))
            b = math.max(0, math.min(1, b))
            setPixel(x, y, r, g, b)
        end
    end

    -- Beat flash overlay
    if beat > 0.6 then
        local flash = (beat - 0.6) * 2
        for y = 0, HEIGHT - 1 do
            for x = 0, WIDTH - 1 do
                setPixelHSV(x, y, hue, 0.3, flash * 0.3)
            end
        end
    end
end
