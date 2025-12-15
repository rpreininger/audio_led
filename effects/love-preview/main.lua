-- LED Effect Preview for LÖVE
-- Simulates the LED matrix API and runs Lua effect scripts

local WIDTH = 128
local HEIGHT = 64
local SCALE = 8

-- Pixel buffer
local pixels = {}
local canvas

-- Effect management
local effects = {}
local currentEffectIndex = 1
local currentEffect = nil

-- Simulated audio data
local audio = {
    spectrum = {},
    volume = 0,
    bass = 0,
    mid = 0,
    high = 0,
    beat = false
}

-- Settings (matches your C++ implementation)
local settings = {
    brightness = 255,
    sensitivity = 1.0,
    noiseThreshold = 0.05
}

-- Time tracking
local totalTime = 0

--------------------------------------------------------------------------------
-- API Implementation (matches your C++ LED matrix API)
--------------------------------------------------------------------------------

function clear()
    for y = 0, HEIGHT - 1 do
        for x = 0, WIDTH - 1 do
            pixels[y][x] = {0, 0, 0}
        end
    end
end

function setPixel(x, y, r, g, b)
    x = math.floor(x)
    y = math.floor(y)
    if x >= 0 and x < WIDTH and y >= 0 and y < HEIGHT then
        pixels[y][x] = {
            math.min(255, math.max(0, math.floor(r))),
            math.min(255, math.max(0, math.floor(g))),
            math.min(255, math.max(0, math.floor(b)))
        }
    end
end

function setPixelHSV(x, y, h, s, v)
    -- Convert HSV to RGB
    h = h % 360
    local c = v * s
    local x_val = c * (1 - math.abs((h / 60) % 2 - 1))
    local m = v - c

    local r, g, b = 0, 0, 0
    if h < 60 then
        r, g, b = c, x_val, 0
    elseif h < 120 then
        r, g, b = x_val, c, 0
    elseif h < 180 then
        r, g, b = 0, c, x_val
    elseif h < 240 then
        r, g, b = 0, x_val, c
    elseif h < 300 then
        r, g, b = x_val, 0, c
    else
        r, g, b = c, 0, x_val
    end

    setPixel(x, y, (r + m) * 255, (g + m) * 255, (b + m) * 255)
end

function drawLine(x1, y1, x2, y2, r, g, b)
    local dx = math.abs(x2 - x1)
    local dy = math.abs(y2 - y1)
    local sx = x1 < x2 and 1 or -1
    local sy = y1 < y2 and 1 or -1
    local err = dx - dy

    while true do
        setPixel(x1, y1, r, g, b)
        if x1 == x2 and y1 == y2 then break end
        local e2 = 2 * err
        if e2 > -dy then
            err = err - dy
            x1 = x1 + sx
        end
        if e2 < dx then
            err = err + dx
            y1 = y1 + sy
        end
    end
end

function fillRect(x, y, w, h, r, g, b)
    for py = y, y + h - 1 do
        for px = x, x + w - 1 do
            setPixel(px, py, r, g, b)
        end
    end
end

--------------------------------------------------------------------------------
-- Simulated Audio Generation
--------------------------------------------------------------------------------

local function generateSimulatedAudio(dt)
    -- Generate fake audio data that looks interesting
    local t = totalTime

    -- Simulate beat detection (roughly 120 BPM)
    local beatPhase = (t * 2) % 1
    audio.beat = beatPhase < 0.1

    -- Simulate bass/mid/high
    audio.bass = 0.3 + 0.5 * math.abs(math.sin(t * 2))
    audio.mid = 0.2 + 0.4 * math.abs(math.sin(t * 3 + 1))
    audio.high = 0.1 + 0.3 * math.abs(math.sin(t * 5 + 2))

    -- Overall volume
    audio.volume = (audio.bass + audio.mid + audio.high) / 3

    -- Spectrum (32 bands)
    for i = 1, 32 do
        local freq = i / 32
        local base = math.sin(t * (1 + freq * 3) + i * 0.5) * 0.5 + 0.5
        local noise = math.random() * 0.1
        audio.spectrum[i] = math.max(0, base * (1 - freq * 0.5) + noise) * 100
    end
end

--------------------------------------------------------------------------------
-- Effect Loading
--------------------------------------------------------------------------------

local function loadEffects()
    effects = {}

    -- Get script directory
    local scriptDir = love.filesystem.getSource() .. "/../scripts"

    -- Use lfs or scan for files
    local files = love.filesystem.getDirectoryItems("../scripts")

    if #files == 0 then
        -- Fallback: hardcode known effects
        files = {
            "colortest.lua",
            "donkey.lua",
            "spacewalk.lua",
            "rainbow_bars.lua",
            "bass_pulse.lua",
            "oscilloscope.lua",
            "superscope.lua",
            "milkdrop.lua",
            "geiss.lua",
            "tripex.lua",
            "dancing_bars.lua",
            "fireblast.lua",
            "pacman.lua",
            "jumpnrun.lua",
            "whacatourist.lua"
        }
    end

    for _, file in ipairs(files) do
        if file:match("%.lua$") then
            local path = scriptDir .. "/" .. file
            local f = io.open(path, "r")
            if f then
                local content = f:read("*all")
                f:close()

                -- Create a sandboxed environment for the effect
                local env = setmetatable({
                    -- API functions
                    clear = clear,
                    setPixel = setPixel,
                    setPixelHSV = setPixelHSV,
                    drawLine = drawLine,
                    fillRect = fillRect,
                    WIDTH = WIDTH,
                    HEIGHT = HEIGHT,
                    -- Standard Lua functions
                    math = math,
                    string = string,
                    table = table,
                    pairs = pairs,
                    ipairs = ipairs,
                    type = type,
                    tonumber = tonumber,
                    tostring = tostring,
                    print = print,
                    unpack = unpack or table.unpack,
                    -- File I/O for sprite loading
                    io = io,
                    os = os,
                    load = load,
                    setmetatable = setmetatable,
                    love = love,
                    _G = _G,
                }, {__index = _G})

                local chunk, err = load(content, file, "t", env)
                if chunk then
                    local ok, loadErr = pcall(chunk)
                    if ok then
                        table.insert(effects, {
                            name = env.effect_name or file,
                            description = env.effect_description or "",
                            env = env,
                            file = file
                        })
                        print("Loaded: " .. (env.effect_name or file))
                    else
                        print("Error running " .. file .. ": " .. tostring(loadErr))
                    end
                else
                    print("Error loading " .. file .. ": " .. tostring(err))
                end
            end
        end
    end

    if #effects > 0 then
        currentEffect = effects[1]
    end
end

local function initCurrentEffect()
    if currentEffect and currentEffect.env.init then
        currentEffect.env.WIDTH = WIDTH
        currentEffect.env.HEIGHT = HEIGHT
        local ok, err = pcall(currentEffect.env.init, WIDTH, HEIGHT)
        if not ok then
            print("Error in init: " .. tostring(err))
        end
    end
end

--------------------------------------------------------------------------------
-- LÖVE Callbacks
--------------------------------------------------------------------------------

function love.load()
    -- Initialize pixel buffer
    for y = 0, HEIGHT - 1 do
        pixels[y] = {}
        for x = 0, WIDTH - 1 do
            pixels[y][x] = {0, 0, 0}
        end
    end

    -- Create canvas for efficient rendering
    canvas = love.graphics.newCanvas(WIDTH, HEIGHT)
    canvas:setFilter("nearest", "nearest")

    -- Load effects
    loadEffects()

    if currentEffect then
        initCurrentEffect()
    end

    print("\n=== LED Effect Preview ===")
    print("Controls:")
    print("  Left/Right: Change effect")
    print("  R: Reset current effect")
    print("  Space: Pause/Resume")
    print("  +/-: Adjust scale")
    print("  ESC: Quit")
    print("")
end

local paused = false

function love.update(dt)
    if paused then return end

    totalTime = totalTime + dt

    -- Generate simulated audio
    generateSimulatedAudio(dt)

    -- Update current effect
    if currentEffect and currentEffect.env.update then
        local ok, err = pcall(currentEffect.env.update, audio, settings, totalTime)
        if not ok then
            print("Error in update: " .. tostring(err))
        end
    end
end

function love.draw()
    -- Render pixels to canvas
    love.graphics.setCanvas(canvas)
    love.graphics.clear(0, 0, 0)

    for y = 0, HEIGHT - 1 do
        for x = 0, WIDTH - 1 do
            local p = pixels[y][x]
            if p[1] > 0 or p[2] > 0 or p[3] > 0 then
                love.graphics.setColor(p[1]/255, p[2]/255, p[3]/255)
                love.graphics.points(x + 0.5, y + 0.5)
            end
        end
    end

    love.graphics.setCanvas()

    -- Draw scaled canvas
    love.graphics.setColor(1, 1, 1)
    local winW, winH = love.graphics.getDimensions()
    local scaleX = winW / WIDTH
    local scaleY = winH / HEIGHT
    local scale = math.min(scaleX, scaleY)
    local offsetX = (winW - WIDTH * scale) / 2
    local offsetY = (winH - HEIGHT * scale) / 2

    love.graphics.draw(canvas, offsetX, offsetY, 0, scale, scale)

    -- Draw UI overlay
    love.graphics.setColor(1, 1, 1, 0.8)
    local info = string.format("Effect %d/%d: %s",
        currentEffectIndex, #effects,
        currentEffect and currentEffect.name or "None")
    love.graphics.print(info, 10, 10)

    if paused then
        love.graphics.print("PAUSED", 10, 30)
    end
end

function love.keypressed(key)
    if key == "escape" then
        love.event.quit()
    elseif key == "left" then
        if #effects > 0 then
            currentEffectIndex = currentEffectIndex - 1
            if currentEffectIndex < 1 then currentEffectIndex = #effects end
            currentEffect = effects[currentEffectIndex]
            clear()
            initCurrentEffect()
        end
    elseif key == "right" then
        if #effects > 0 then
            currentEffectIndex = currentEffectIndex + 1
            if currentEffectIndex > #effects then currentEffectIndex = 1 end
            currentEffect = effects[currentEffectIndex]
            clear()
            initCurrentEffect()
        end
    elseif key == "r" then
        clear()
        initCurrentEffect()
    elseif key == "space" then
        paused = not paused
    end
end

function love.resize(w, h)
    -- Window was resized, nothing special needed
end
