-- ====================================================================
-- RAINBOW BARS EFFECT - Lua scripted effect
-- Spectrum bars with animated rainbow colors
-- ====================================================================

effect_name = "Rainbow Bars (Lua)"
effect_description = "Spectrum analyzer with animated rainbow"

-- Local state
local smoothed = {0, 0, 0, 0, 0, 0, 0, 0}
local colorOffset = 0

function init(width, height)
    for i = 1, 8 do
        smoothed[i] = 0
    end
    colorOffset = 0
end

function reset()
    init(WIDTH, HEIGHT)
end

function update(audio, settings, time)
    clear()

    -- Animate color offset
    colorOffset = colorOffset + 1
    if colorOffset >= 360 then colorOffset = 0 end

    local barWidth = WIDTH / 8
    local threshold = settings.noiseThreshold

    for i = 1, 8 do
        -- Get spectrum value (Lua arrays are 1-indexed)
        local val = audio.spectrum[i] or 0
        if val < threshold then val = 0 end

        -- Smooth the value
        if val > smoothed[i] then
            smoothed[i] = val
        else
            smoothed[i] = smoothed[i] * 0.9 + val * 0.1
        end

        -- Calculate bar height
        local barHeight = math.floor(smoothed[i] * 0.8)
        if barHeight > HEIGHT then barHeight = HEIGHT end

        -- Calculate bar position
        local x1 = math.floor((i - 1) * barWidth) + 2
        local x2 = math.floor(i * barWidth) - 2

        -- Calculate color with offset
        local hue = ((i - 1) * 45 + colorOffset) % 360

        -- Draw bar from bottom up
        for y = HEIGHT - barHeight, HEIGHT - 1 do
            for x = x1, x2 do
                -- Gradient: brighter at top
                local brightness = 0.5 + 0.5 * (HEIGHT - y) / HEIGHT
                setPixelHSV(x, y, hue, 1, brightness)
            end
        end
    end
end
