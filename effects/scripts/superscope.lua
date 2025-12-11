-- ====================================================================
-- SUPERSCOPE - Winamp AVS-style circular waveform
-- ====================================================================

effect_name = "Superscope (Lua)"
effect_description = "AVS-style spiraling audio visualization"

local rotation = 0
local hue = 0
local trail = {}
local trailLength = 8

function init(width, height)
    rotation = 0
    hue = 0
    trail = {}
end

function reset()
    init(WIDTH, HEIGHT)
end

function update(audio, settings, time)
    -- Fade effect (darken previous frame)
    for y = 0, HEIGHT - 1 do
        for x = 0, WIDTH - 1 do
            setPixel(x, y, 0, 0, 0)
        end
    end

    local vol = audio.volume or 0
    local beat = audio.beat or 0
    local bass = audio.bass or 0

    if vol < settings.noiseThreshold then vol = 0 end

    -- Rotation speed based on audio
    rotation = rotation + 0.02 + vol * 0.05 + beat * 0.1
    hue = hue + 1 + beat * 5
    if hue >= 360 then hue = hue - 360 end

    local cx = WIDTH / 2
    local cy = HEIGHT / 2

    -- Draw multiple spiral arms
    local numArms = 3
    for arm = 0, numArms - 1 do
        local armOffset = arm * (2 * math.pi / numArms)
        local armHue = (hue + arm * 120) % 360

        local prevX, prevY = nil, nil

        -- Points along the spiral
        for i = 0, 64 do
            local t = i / 64
            local angle = rotation + armOffset + t * math.pi * 4

            -- Radius modulated by spectrum
            local bandIndex = math.floor(t * 7) + 1
            local specVal = audio.spectrum[bandIndex] or 0
            local baseRadius = 5 + t * 25
            local radius = baseRadius + specVal * 20 + bass * t * 15

            -- Spiral coordinates
            local x = cx + math.cos(angle) * radius
            local y = cy + math.sin(angle) * radius * 0.7  -- Squash for aspect

            if x >= 0 and x < WIDTH and y >= 0 and y < HEIGHT then
                -- Draw with intensity based on position
                local intensity = 0.5 + t * 0.5
                setPixelHSV(math.floor(x), math.floor(y), armHue, 1, intensity)

                -- Connect points with lines
                if prevX and prevY then
                    local dx = x - prevX
                    local dy = y - prevY
                    local steps = math.max(math.abs(dx), math.abs(dy))
                    if steps > 0 then
                        for s = 0, steps do
                            local lx = prevX + dx * s / steps
                            local ly = prevY + dy * s / steps
                            if lx >= 0 and lx < WIDTH and ly >= 0 and ly < HEIGHT then
                                setPixelHSV(math.floor(lx), math.floor(ly), armHue, 1, intensity * 0.8)
                            end
                        end
                    end
                end

                prevX, prevY = x, y
            end
        end
    end

    -- Center glow based on beat
    if beat > 0.3 then
        local glowSize = 3 + math.floor(beat * 8)
        for dy = -glowSize, glowSize do
            for dx = -glowSize, glowSize do
                local dist = math.sqrt(dx * dx + dy * dy)
                if dist <= glowSize then
                    local px = math.floor(cx + dx)
                    local py = math.floor(cy + dy)
                    if px >= 0 and px < WIDTH and py >= 0 and py < HEIGHT then
                        local intensity = (1 - dist / glowSize) * beat
                        setPixelHSV(px, py, hue, 0.5, intensity)
                    end
                end
            end
        end
    end
end
