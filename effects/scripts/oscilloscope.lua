-- ====================================================================
-- OSCILLOSCOPE - Classic Winamp-style waveform display
-- ====================================================================

effect_name = "Oscilloscope (Lua)"
effect_description = "Classic Winamp-style dual oscilloscope"

local phase = 0
local hue = 0

function init(width, height)
    phase = 0
    hue = 0
end

function reset()
    phase = 0
    hue = 0
end

function update(audio, settings, time)
    clear()

    local vol = audio.volume or 0
    local beat = audio.beat or 0
    local bass = audio.bass or 0
    local mid = audio.mid or 0
    local treble = audio.treble or 0

    if vol < settings.noiseThreshold then vol = 0 end

    -- Animate
    phase = phase + 0.1 + vol * 0.2
    hue = hue + 0.5
    if hue >= 360 then hue = hue - 360 end

    -- Draw center line (dim)
    local cy = HEIGHT / 2
    for x = 0, WIDTH - 1 do
        setPixelHSV(x, math.floor(cy), hue, 0.3, 0.2)
    end

    -- Upper waveform (bass + mid)
    local prevY1 = cy
    for x = 0, WIDTH - 1 do
        local t = x / WIDTH * math.pi * 4
        local wave = math.sin(t + phase) * bass * 20
        wave = wave + math.sin(t * 2.5 + phase * 1.3) * mid * 15
        wave = wave + math.sin(t * 0.5 + phase * 0.7) * vol * 10

        local y1 = cy - 8 - wave
        if y1 < 0 then y1 = 0 end
        if y1 >= HEIGHT then y1 = HEIGHT - 1 end

        -- Draw line segment
        local minY = math.min(prevY1, y1)
        local maxY = math.max(prevY1, y1)
        for y = math.floor(minY), math.floor(maxY) do
            if y >= 0 and y < HEIGHT then
                local intensity = 0.7 + beat * 0.3
                setPixelHSV(x, y, (hue + x * 0.5) % 360, 1, intensity)
            end
        end
        setPixelHSV(x, math.floor(y1), (hue + x * 0.5) % 360, 1, 1)
        prevY1 = y1
    end

    -- Lower waveform (treble + mid) - different color
    local prevY2 = cy
    for x = 0, WIDTH - 1 do
        local t = x / WIDTH * math.pi * 6
        local wave = math.sin(t - phase * 1.2) * treble * 18
        wave = wave + math.sin(t * 1.8 - phase) * mid * 12
        wave = wave + math.cos(t * 0.8 + phase * 0.5) * vol * 8

        local y2 = cy + 8 + wave
        if y2 < 0 then y2 = 0 end
        if y2 >= HEIGHT then y2 = HEIGHT - 1 end

        local minY = math.min(prevY2, y2)
        local maxY = math.max(prevY2, y2)
        for y = math.floor(minY), math.floor(maxY) do
            if y >= 0 and y < HEIGHT then
                local intensity = 0.6 + beat * 0.4
                setPixelHSV(x, y, (hue + 180 + x * 0.5) % 360, 1, intensity)
            end
        end
        setPixelHSV(x, math.floor(y2), (hue + 180 + x * 0.5) % 360, 1, 1)
        prevY2 = y2
    end

    -- Beat flash in corners
    if beat > 0.5 then
        local flashSize = math.floor(beat * 10)
        for i = 0, flashSize do
            setPixelHSV(i, i, hue, 1, beat)
            setPixelHSV(WIDTH - 1 - i, i, hue, 1, beat)
            setPixelHSV(i, HEIGHT - 1 - i, hue, 1, beat)
            setPixelHSV(WIDTH - 1 - i, HEIGHT - 1 - i, hue, 1, beat)
        end
    end
end
