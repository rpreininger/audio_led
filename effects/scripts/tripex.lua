-- ====================================================================
-- TRIPEX - Geometric tunnel with spectrum-reactive walls
-- ====================================================================

effect_name = "Tripex (Lua)"
effect_description = "Geometric tunnel flying through audio"

local depth = 0
local rotation = 0
local hue = 0

function init(width, height)
    depth = 0
    rotation = 0
    hue = 0
end

function reset()
    init(WIDTH, HEIGHT)
end

function update(audio, settings, time)
    clear()

    local vol = audio.volume or 0
    local beat = audio.beat and 1 or 0
    local bass = audio.bass or 0

    if vol < settings.noiseThreshold then vol = 0 end

    -- Movement speed
    depth = depth + 2 + vol * 5 + beat * 10
    rotation = rotation + 0.01 + vol * 0.02
    hue = hue + 0.5 + beat * 3
    if hue >= 360 then hue = hue - 360 end

    local cx = WIDTH / 2
    local cy = HEIGHT / 2

    -- Draw tunnel rings from back to front
    local numRings = 12
    for ring = numRings, 1, -1 do
        local z = ring * 30 + (depth % 30)
        local scale = 300 / (z + 10)

        -- Ring size and position
        local ringSize = scale * 40
        local sides = 6  -- Hexagon tunnel

        -- Calculate vertices
        local verts = {}
        for i = 0, sides - 1 do
            local angle = rotation + i * (2 * math.pi / sides)

            -- Spectrum modulation per side
            local specIndex = (i % 8) + 1
            local specMod = 1 + (audio.spectrum[specIndex] or 0) * 0.5

            local r = ringSize * specMod
            local vx = cx + math.cos(angle) * r
            local vy = cy + math.sin(angle) * r * 0.6  -- Aspect correction

            table.insert(verts, {x = vx, y = vy})
        end

        -- Draw ring edges
        local ringHue = (hue + ring * 15) % 360
        local brightness = (numRings - ring + 1) / numRings
        brightness = brightness * (0.4 + vol * 0.4)

        for i = 1, #verts do
            local v1 = verts[i]
            local v2 = verts[(i % #verts) + 1]

            -- Draw line between vertices
            local dx = v2.x - v1.x
            local dy = v2.y - v1.y
            local steps = math.max(math.abs(dx), math.abs(dy))

            if steps > 0 then
                for s = 0, steps do
                    local px = v1.x + dx * s / steps
                    local py = v1.y + dy * s / steps

                    if px >= 0 and px < WIDTH and py >= 0 and py < HEIGHT then
                        setPixelHSV(math.floor(px), math.floor(py), ringHue, 1, brightness)
                    end
                end
            end
        end

        -- Draw corner highlights
        for i, v in ipairs(verts) do
            if v.x >= 0 and v.x < WIDTH and v.y >= 0 and v.y < HEIGHT then
                setPixelHSV(math.floor(v.x), math.floor(v.y), ringHue, 0.5, brightness * 1.5)
            end
        end
    end

    -- Center glow
    local glowSize = 3 + math.floor(beat * 5)
    for dy = -glowSize, glowSize do
        for dx = -glowSize, glowSize do
            local dist = math.sqrt(dx * dx + dy * dy)
            if dist <= glowSize then
                local px = math.floor(cx + dx)
                local py = math.floor(cy + dy)
                if px >= 0 and px < WIDTH and py >= 0 and py < HEIGHT then
                    local intensity = (1 - dist / glowSize) * (0.5 + beat * 0.5)
                    setPixelHSV(px, py, hue, 0.3, intensity)
                end
            end
        end
    end
end
