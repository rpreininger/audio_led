-- ====================================================================
-- DANCING BARS - Classic Winamp spectrum with style
-- ====================================================================

effect_name = "Dancing Bars (Lua)"
effect_description = "Classic spectrum analyzer with peak hold and glow"

local peaks = {0, 0, 0, 0, 0, 0, 0, 0}
local peakHold = {0, 0, 0, 0, 0, 0, 0, 0}
local smoothed = {0, 0, 0, 0, 0, 0, 0, 0}
local hue = 0

function init(width, height)
    for i = 1, 8 do
        peaks[i] = 0
        peakHold[i] = 0
        smoothed[i] = 0
    end
    hue = 0
end

function reset()
    init(WIDTH, HEIGHT)
end

function update(audio, settings, time)
    clear()

    local vol = audio.volume or 0
    local beat = audio.beat or 0

    hue = hue + 0.3 + beat * 2
    if hue >= 360 then hue = hue - 360 end

    local barWidth = math.floor(WIDTH / 8) - 2
    local maxHeight = HEIGHT - 4

    for i = 1, 8 do
        local specVal = audio.spectrum[i] or 0
        if specVal < settings.noiseThreshold then specVal = 0 end

        -- Smooth the value
        if specVal > smoothed[i] then
            smoothed[i] = specVal
        else
            smoothed[i] = smoothed[i] * 0.85 + specVal * 0.15
        end

        -- Peak hold
        if smoothed[i] > peaks[i] then
            peaks[i] = smoothed[i]
            peakHold[i] = 30  -- Hold frames
        else
            peakHold[i] = peakHold[i] - 1
            if peakHold[i] <= 0 then
                peaks[i] = peaks[i] * 0.95
            end
        end

        local barH = math.floor(smoothed[i] * maxHeight * 0.8)
        local peakY = math.floor(peaks[i] * maxHeight * 0.8)

        local barX = (i - 1) * (barWidth + 2) + 1

        -- Bar color - rainbow gradient
        local barHue = (hue + (i - 1) * 45) % 360

        -- Draw main bar with gradient
        for y = HEIGHT - 1, HEIGHT - barH, -1 do
            local yRatio = (HEIGHT - y) / maxHeight
            local brightness = 0.4 + yRatio * 0.6

            -- Glow effect at edges
            for x = barX, barX + barWidth - 1 do
                local xDist = math.min(x - barX, barX + barWidth - 1 - x)
                local edgeBright = brightness
                if xDist == 0 then
                    edgeBright = brightness * 1.3  -- Edge highlight
                end

                if x >= 0 and x < WIDTH and y >= 0 and y < HEIGHT then
                    setPixelHSV(x, y, barHue, 0.9, math.min(1, edgeBright))
                end
            end
        end

        -- Draw peak marker
        local peakMarkerY = HEIGHT - peakY - 1
        if peakMarkerY >= 0 and peakMarkerY < HEIGHT then
            for x = barX, barX + barWidth - 1 do
                if x >= 0 and x < WIDTH then
                    setPixelHSV(x, peakMarkerY, barHue, 0.5, 1)
                    if peakMarkerY + 1 < HEIGHT then
                        setPixelHSV(x, peakMarkerY + 1, barHue, 0.5, 0.5)
                    end
                end
            end
        end

        -- Reflection at bottom (subtle)
        local reflectH = math.floor(barH * 0.3)
        for ry = 0, reflectH - 1 do
            local y = HEIGHT - 1 - ry
            local reflectBright = 0.15 * (1 - ry / reflectH)
            for x = barX, barX + barWidth - 1 do
                if x >= 0 and x < WIDTH and y >= 0 and y < HEIGHT then
                    setPixelHSV(x, HEIGHT - 1, barHue, 0.9, reflectBright)
                end
            end
        end
    end

    -- Beat flash line at bottom
    if beat > 0.5 then
        for x = 0, WIDTH - 1 do
            setPixelHSV(x, HEIGHT - 1, hue, 1, beat * 0.8)
        end
    end
end
