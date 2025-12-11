-- ====================================================================
-- PACMAN EFFECT - Lua scripted effect
-- Audio-reactive Pacman with ghosts
-- ====================================================================

effect_name = "Pacman (Lua)"
effect_description = "Pacman and ghosts react to audio"

-- Pacman state
local pacX = 0
local pacDir = 1  -- 1 = right, -1 = left
local mouthAngle = 0
local mouthOpen = true

-- Ghost positions and colors
local ghosts = {
    {x = 100, color = {1, 0, 0}},      -- Red (Blinky)
    {x = 85, color = {1, 0.5, 1}},     -- Pink (Pinky)
    {x = 70, color = {0, 1, 1}},       -- Cyan (Inky)
    {x = 55, color = {1, 0.5, 0}}      -- Orange (Clyde)
}

-- Dots
local dots = {}
local dotSpacing = 8

function init(width, height)
    pacX = 10
    -- Initialize dots
    dots = {}
    for x = 4, WIDTH - 4, dotSpacing do
        table.insert(dots, {x = x, eaten = false})
    end
end

function reset()
    init(WIDTH, HEIGHT)
end

-- Draw Pacman (yellow circle with mouth)
function drawPacman(cx, cy, radius, mouthDeg, direction)
    local startAngle = mouthDeg / 2
    local endAngle = 360 - mouthDeg / 2

    if direction < 0 then
        -- Facing left
        startAngle = 180 + mouthDeg / 2
        endAngle = 180 - mouthDeg / 2 + 360
    end

    for y = cy - radius, cy + radius do
        for x = cx - radius, cx + radius do
            local dx = x - cx
            local dy = y - cy
            local dist = math.sqrt(dx * dx + dy * dy)

            if dist <= radius then
                -- Check if in mouth area
                local angle = math.deg(math.atan(dy, dx))
                if angle < 0 then angle = angle + 360 end

                local inMouth = false
                if direction > 0 then
                    inMouth = angle < startAngle or angle > endAngle
                else
                    inMouth = angle > startAngle and angle < endAngle
                end

                if not inMouth and x >= 0 and x < WIDTH and y >= 0 and y < HEIGHT then
                    setPixelHSV(math.floor(x), math.floor(y), 60, 1, 1)  -- Yellow
                end
            end
        end
    end

    -- Eye
    local eyeX = cx + direction * 3
    local eyeY = cy - radius / 2
    if eyeX >= 0 and eyeX < WIDTH and eyeY >= 0 and eyeY < HEIGHT then
        setPixel(math.floor(eyeX), math.floor(eyeY), 0, 0, 0)
    end
end

-- Draw Ghost
function drawGhost(cx, cy, radius, r, g, b, scared)
    -- Body (rounded top, wavy bottom)
    for y = cy - radius, cy + radius do
        for x = cx - radius, cx + radius do
            local dx = x - cx
            local dy = y - cy

            -- Top half is circular
            if dy < 0 then
                local dist = math.sqrt(dx * dx + dy * dy)
                if dist <= radius and x >= 0 and x < WIDTH and y >= 0 and y < HEIGHT then
                    if scared then
                        setPixel(math.floor(x), math.floor(y), 0, 0, 200)  -- Blue when scared
                    else
                        setPixel(math.floor(x), math.floor(y),
                                math.floor(r * 255), math.floor(g * 255), math.floor(b * 255))
                    end
                end
            else
                -- Bottom half is rectangular with wavy edge
                if math.abs(dx) <= radius then
                    local waveOffset = math.floor(math.sin((x + cy) * 0.5) * 2)
                    if dy <= radius + waveOffset and x >= 0 and x < WIDTH and y >= 0 and y < HEIGHT then
                        if scared then
                            setPixel(math.floor(x), math.floor(y), 0, 0, 200)
                        else
                            setPixel(math.floor(x), math.floor(y),
                                    math.floor(r * 255), math.floor(g * 255), math.floor(b * 255))
                        end
                    end
                end
            end
        end
    end

    -- Eyes
    local eyeOffsets = {-3, 3}
    for _, ox in ipairs(eyeOffsets) do
        local ex = cx + ox
        local ey = cy - 2
        if ex >= 0 and ex < WIDTH and ey >= 0 and ey < HEIGHT then
            setPixel(math.floor(ex), math.floor(ey), 255, 255, 255)
            setPixel(math.floor(ex + 1), math.floor(ey), 0, 0, 0)
        end
    end
end

function update(audio, settings, time)
    clear()

    local beat = audio.beat or 0
    local vol = audio.volume or 0
    local bass = audio.bass or 0

    -- Draw dots (small yellow circles)
    local cy = HEIGHT / 2
    for _, dot in ipairs(dots) do
        if not dot.eaten then
            local dx = math.abs(dot.x - pacX)
            if dx < 6 then
                dot.eaten = true
            else
                setPixelHSV(math.floor(dot.x), math.floor(cy), 60, 1, 0.8)
                setPixelHSV(math.floor(dot.x), math.floor(cy - 1), 60, 1, 0.8)
            end
        end
    end

    -- Move Pacman based on volume
    local speed = 0.3 + vol * 1.5
    pacX = pacX + speed * pacDir

    -- Bounce at edges
    if pacX > WIDTH - 10 then
        pacDir = -1
        -- Reset dots when reaching edge
        for _, dot in ipairs(dots) do
            dot.eaten = false
        end
    elseif pacX < 10 then
        pacDir = 1
        for _, dot in ipairs(dots) do
            dot.eaten = false
        end
    end

    -- Animate mouth based on beat
    local mouthDeg = 10 + beat * 50
    if mouthDeg > 60 then mouthDeg = 60 end

    -- Pacman size reacts to bass
    local pacRadius = 8 + bass * 5
    if pacRadius > 15 then pacRadius = 15 end

    -- Draw Pacman
    drawPacman(pacX, cy, pacRadius, mouthDeg, pacDir)

    -- Update and draw ghosts
    local scared = beat > 0.5  -- Ghosts scared on strong beats

    for i, ghost in ipairs(ghosts) do
        -- Ghosts follow Pacman with delay, speed based on spectrum
        local targetX = pacX - pacDir * (20 + i * 15)
        local ghostSpeed = 0.1 + (audio.spectrum[i] or 0) * 0.3

        if ghost.x < targetX then
            ghost.x = ghost.x + ghostSpeed
        else
            ghost.x = ghost.x - ghostSpeed
        end

        -- Keep ghosts on screen
        if ghost.x < 10 then ghost.x = 10 end
        if ghost.x > WIDTH - 10 then ghost.x = WIDTH - 10 end

        -- Ghost size reacts to its spectrum band
        local ghostRadius = 6 + (audio.spectrum[i + 4] or 0) * 3
        if ghostRadius > 10 then ghostRadius = 10 end

        drawGhost(ghost.x, cy, ghostRadius, ghost.color[1], ghost.color[2], ghost.color[3], scared)
    end
end
