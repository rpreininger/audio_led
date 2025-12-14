-- ====================================================================
-- SPACEWALK - Space Control Room Effect
-- View astronauts floating through space from the spaceship deck
-- ====================================================================

effect_name = "Spacewalk"
effect_description = "Astronauts floating in space seen from spaceship deck"

-- Sprite loading (will be populated in init)
local deck_data = nil
local astronauts = {}

-- Stars
local stars = {}
local NUM_STARS = 60

-- Astronaut state
local astro_objects = {}

-- Time tracking
local lastTime = 0

--------------------------------------------------------------------------------
-- Load sprite file
--------------------------------------------------------------------------------
local function loadSpriteFile(filename)
    -- Try multiple paths
    local paths = {
        "effects/scripts/sprites/" .. filename,
        "scripts/sprites/" .. filename,
        "../scripts/sprites/" .. filename,
    }

    -- Add LÖVE-specific path (works on Windows)
    if love and love.filesystem then
        local src = love.filesystem.getSource()
        -- Replace backslashes with forward slashes for consistency
        src = src:gsub("\\", "/")
        table.insert(paths, 1, src .. "/../scripts/sprites/" .. filename)
        -- Also try Windows-style path
        table.insert(paths, 2, src .. "\\..\\scripts\\sprites\\" .. filename)
    end

    -- Add hardcoded Windows fallback path
    table.insert(paths, "D:/Developer/C++/raspi/effects/scripts/sprites/" .. filename)
    table.insert(paths, "D:\\Developer\\C++\\raspi\\effects\\scripts\\sprites\\" .. filename)

    local content = nil
    local usedPath = nil

    for _, path in ipairs(paths) do
        local f = io.open(path, "r")
        if f then
            content = f:read("*all")
            f:close()
            usedPath = path
            print("Loaded sprite from: " .. path)
            break
        end
    end

    if not content then
        print("ERROR: Could not load sprite: " .. filename)
        return nil
    end

    -- Create environment for the sprite
    local env = setmetatable({
        WIDTH = WIDTH,
        HEIGHT = HEIGHT,
        setPixel = setPixel,
        math = math,
        ipairs = ipairs,
    }, {__index = _G})

    local chunk, err = load(content, filename, "t", env)
    if not chunk then
        print("ERROR loading " .. filename .. ": " .. tostring(err))
        return nil
    end

    local ok, result = pcall(chunk)
    if not ok then
        print("ERROR running " .. filename .. ": " .. tostring(result))
        return nil
    end

    -- The sprite file returns a table with palette, sprite, etc.
    if type(result) ~= "table" then
        print("ERROR: Sprite file did not return a table: " .. filename)
        return nil
    end

    if not result.palette then
        print("ERROR: No palette in returned table for " .. filename)
        return nil
    end
    if not result.sprite then
        print("ERROR: No sprite in returned table for " .. filename)
        return nil
    end

    print("Sprite " .. filename .. " loaded: " .. #result.sprite .. " rows, " .. #result.palette .. " colors")

    return result
end

--------------------------------------------------------------------------------
-- Draw sprite with alpha (skip palette index 0)
--------------------------------------------------------------------------------
local function drawSpriteAlpha(sprite_data, x, y, scale)
    if not sprite_data or not sprite_data.sprite or not sprite_data.palette then
        return
    end

    local sprite = sprite_data.sprite
    local palette = sprite_data.palette
    scale = scale or 1

    for row_idx, row in ipairs(sprite) do
        for col_idx, color_idx in ipairs(row) do
            if color_idx ~= 0 then  -- Skip transparent pixels
                local color = palette[color_idx + 1]
                if color then
                    if scale == 1 then
                        local px = math.floor(x + col_idx - 1)
                        local py = math.floor(y + row_idx - 1)
                        if px >= 0 and px < WIDTH and py >= 0 and py < HEIGHT then
                            setPixel(px, py, color[1], color[2], color[3])
                        end
                    else
                        -- Scaled drawing
                        for sy = 0, scale - 1 do
                            for sx = 0, scale - 1 do
                                local px = math.floor(x + (col_idx - 1) * scale + sx)
                                local py = math.floor(y + (row_idx - 1) * scale + sy)
                                if px >= 0 and px < WIDTH and py >= 0 and py < HEIGHT then
                                    setPixel(px, py, color[1], color[2], color[3])
                                end
                            end
                        end
                    end
                end
            end
        end
    end
end

--------------------------------------------------------------------------------
-- Draw sprite scaled (no rotation, cleaner pixels)
--------------------------------------------------------------------------------
local function drawSpriteScaledFloat(sprite_data, x, y, scale)
    if not sprite_data or not sprite_data.sprite or not sprite_data.palette then
        return
    end

    local sprite = sprite_data.sprite
    local palette = sprite_data.palette

    local spriteH = #sprite
    local spriteW = sprite[1] and #sprite[1] or 0

    local destW = math.floor(spriteW * scale)
    local destH = math.floor(spriteH * scale)

    for dy = 0, destH - 1 do
        for dx = 0, destW - 1 do
            local srcX = math.floor(dx / scale) + 1
            local srcY = math.floor(dy / scale) + 1

            if srcY <= spriteH and srcX <= spriteW then
                local row = sprite[srcY]
                if row then
                    local color_idx = row[srcX]
                    if color_idx and color_idx ~= 0 then
                        local color = palette[color_idx + 1]
                        if color then
                            local px = math.floor(x + dx)
                            local py = math.floor(y + dy)
                            if px >= 0 and px < WIDTH and py >= 0 and py < HEIGHT then
                                setPixel(px, py, color[1], color[2], color[3])
                            end
                        end
                    end
                end
            end
        end
    end
end

--------------------------------------------------------------------------------
-- Draw sprite scaled and rotated (for weightless astronauts)
--------------------------------------------------------------------------------
local function drawSpriteRotated(sprite_data, x, y, scale, angle)
    if not sprite_data or not sprite_data.sprite or not sprite_data.palette then
        return
    end

    local sprite = sprite_data.sprite
    local palette = sprite_data.palette

    local spriteH = #sprite
    local spriteW = sprite[1] and #sprite[1] or 0

    -- Center of sprite
    local cx = spriteW / 2
    local cy = spriteH / 2

    local cosA = math.cos(angle)
    local sinA = math.sin(angle)

    -- Calculate bounding box for rotated sprite
    local maxDim = math.ceil(math.sqrt(spriteW * spriteW + spriteH * spriteH) * scale)

    -- Draw rotated and scaled sprite
    for dy = -maxDim/2, maxDim/2 do
        for dx = -maxDim/2, maxDim/2 do
            -- Reverse rotation to find source pixel
            local srcX = (dx * cosA + dy * sinA) / scale + cx
            local srcY = (-dx * sinA + dy * cosA) / scale + cy

            local srcXi = math.floor(srcX) + 1
            local srcYi = math.floor(srcY) + 1

            if srcYi >= 1 and srcYi <= spriteH and srcXi >= 1 and srcXi <= spriteW then
                local row = sprite[srcYi]
                if row then
                    local color_idx = row[srcXi]
                    if color_idx and color_idx ~= 0 then
                        local color = palette[color_idx + 1]
                        if color then
                            local px = math.floor(x + cx * scale + dx)
                            local py = math.floor(y + cy * scale + dy)
                            if px >= 0 and px < WIDTH and py >= 0 and py < HEIGHT then
                                setPixel(px, py, color[1], color[2], color[3])
                            end
                        end
                    end
                end
            end
        end
    end
end

--------------------------------------------------------------------------------
-- Initialize stars
--------------------------------------------------------------------------------
local function initStars()
    stars = {}
    for i = 1, NUM_STARS do
        stars[i] = {
            x = math.random(0, WIDTH - 1),
            y = math.random(0, HEIGHT - 1),
            brightness = math.random(100, 255),
            twinkle = math.random() * 6.28,
            speed = 0.2 + math.random() * 0.5
        }
    end
end

--------------------------------------------------------------------------------
-- Initialize astronauts
--------------------------------------------------------------------------------
local function initAstronauts()
    astro_objects = {}

    local astro_files = {"captain_64.lua", "m9v_64.lua", "stylo_64.lua"}

    for i, filename in ipairs(astro_files) do
        local data = loadSpriteFile(filename)
        if data then
            astronauts[i] = data

            -- Create floating astronaut object with weightless motion
            -- Spread them out across the screen with different zones
            local zone_width = WIDTH / 3
            local start_x = (i - 1) * zone_width + math.random(0, math.floor(zone_width * 0.6))
            local start_y = math.random(-20, HEIGHT - 40)

            astro_objects[i] = {
                sprite_idx = i,
                x = start_x,
                y = start_y,
                -- Very slow drifting velocities for weightless feel
                vx = (math.random() - 0.5) * 6,
                vy = (math.random() - 0.5) * 3,
                -- Z-depth as actual position with velocity (like x,y)
                scale = 0.3 + math.random() * 0.5,  -- Current scale 0.3-0.8
                vz = (math.random() - 0.5) * 0.15,  -- Z velocity (scale change rate)
                -- Actual rotation angle with velocity
                angle = math.random() * 6.28,
                rot_speed = (math.random() - 0.5) * 0.2,  -- Slow rotation
                -- Wobble for floaty motion - different frequencies
                wobble_x = math.random() * 6.28,
                wobble_y = math.random() * 6.28,
                wobble_z = math.random() * 6.28,  -- Wobble for z too
                wobble_speed_x = 0.3 + math.random() * 0.4,
                wobble_speed_y = 0.2 + math.random() * 0.3,
                wobble_speed_z = 0.1 + math.random() * 0.2
            }
        end
    end
end

--------------------------------------------------------------------------------
-- Effect API
--------------------------------------------------------------------------------

function init(width, height)
    print("=== SPACEWALK INIT ===")
    print("WIDTH=" .. tostring(WIDTH) .. " HEIGHT=" .. tostring(HEIGHT))

    math.randomseed(os.time and os.time() or 12345)

    -- Load deck sprite
    print("Loading deck sprite...")
    deck_data = loadSpriteFile("deck.lua")
    if deck_data then
        print("Deck loaded successfully!")
    else
        print("FAILED to load deck!")
    end

    -- Initialize stars
    initStars()
    print("Stars initialized: " .. #stars)

    -- Initialize astronauts
    print("Loading astronauts...")
    initAstronauts()
    print("Astronauts loaded: " .. #astro_objects)

    lastTime = 0
    print("=== INIT COMPLETE ===")
end

function reset()
    init(WIDTH, HEIGHT)
end

function update(audio, settings, time)
    local dt = time - lastTime
    if dt <= 0 or dt > 0.1 then dt = 0.016 end
    lastTime = time

    -- Clear to deep space black
    clear()

    -- Draw twinkling stars
    for i, star in ipairs(stars) do
        star.twinkle = star.twinkle + dt * 3
        local brightness = star.brightness * (0.5 + 0.5 * math.sin(star.twinkle))
        brightness = math.floor(brightness)

        -- Slowly drift stars
        star.x = star.x - star.speed * dt * 10
        if star.x < 0 then
            star.x = WIDTH - 1
            star.y = math.random(0, HEIGHT - 1)
        end

        setPixel(math.floor(star.x), math.floor(star.y), brightness, brightness, brightness)
    end

    -- Update and draw astronauts with weightless motion
    for i, astro in ipairs(astro_objects) do
        -- Update wobble phases (different speeds for x, y, z)
        astro.wobble_x = astro.wobble_x + dt * astro.wobble_speed_x
        astro.wobble_y = astro.wobble_y + dt * astro.wobble_speed_y
        astro.wobble_z = astro.wobble_z + dt * astro.wobble_speed_z

        -- Weightless floating motion - gentle sinusoidal drift in all axes
        local wobble_offset_x = math.sin(astro.wobble_x) * 0.8
        local wobble_offset_y = math.sin(astro.wobble_y) * 0.5
        local wobble_offset_z = math.sin(astro.wobble_z) * 0.02  -- Small z wobble

        -- Update positions with velocity + wobble
        astro.x = astro.x + (astro.vx + wobble_offset_x) * dt
        astro.y = astro.y + (astro.vy + wobble_offset_y) * dt
        astro.scale = astro.scale + (astro.vz + wobble_offset_z) * dt

        -- Bounce z (scale) at limits - reverse direction like bouncing off invisible walls
        if astro.scale < 0.25 then
            astro.scale = 0.25
            astro.vz = math.abs(astro.vz) * 0.8 + math.random() * 0.05  -- Bounce back, coming closer
        elseif astro.scale > 0.95 then
            astro.scale = 0.95
            astro.vz = -math.abs(astro.vz) * 0.8 - math.random() * 0.05  -- Bounce back, going away
        end

        -- Occasionally change z direction randomly (weightless drift)
        if math.random() < 0.002 then
            astro.vz = astro.vz + (math.random() - 0.5) * 0.05
        end

        local current_scale = astro.scale

        -- Wrap around when off screen
        local sprite_data = astronauts[astro.sprite_idx]
        if sprite_data and sprite_data.sprite then
            local spriteW = sprite_data.sprite[1] and #sprite_data.sprite[1] or 30
            local spriteH = #sprite_data.sprite or 64
            local scaledW = spriteW * current_scale
            local scaledH = spriteH * current_scale

            -- Horizontal wrapping with full randomization
            if astro.x > WIDTH + scaledW + 20 then
                astro.x = -scaledW - 20
                astro.y = math.random(-20, HEIGHT - 50)
                astro.scale = 0.3 + math.random() * 0.5
                astro.vx = 2 + math.random() * 4
                astro.vz = (math.random() - 0.5) * 0.15
                astro.rot_speed = (math.random() - 0.5) * 0.2
            elseif astro.x < -scaledW - 20 then
                astro.x = WIDTH + scaledW + 20
                astro.y = math.random(-20, HEIGHT - 50)
                astro.scale = 0.3 + math.random() * 0.5
                astro.vx = -2 - math.random() * 4
                astro.vz = (math.random() - 0.5) * 0.15
                astro.rot_speed = (math.random() - 0.5) * 0.2
            end

            -- Vertical wrapping
            if astro.y > HEIGHT + scaledH then
                astro.y = -scaledH
                astro.vy = math.random() * 2
            elseif astro.y < -scaledH - 20 then
                astro.y = HEIGHT
                astro.vy = -math.random() * 2
            end

            -- Draw astronaut with scale only (no rotation)
            drawSpriteScaledFloat(sprite_data, astro.x, astro.y, current_scale)
        end
    end

    -- Draw deck overlay (foreground with transparency)
    if deck_data then
        drawSpriteAlpha(deck_data, 0, 0, 1)
    end
end
