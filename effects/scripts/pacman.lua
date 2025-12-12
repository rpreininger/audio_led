-- ====================================================================
-- PACMAN - Classic Arcade LED Effect
-- Autonomous AI-controlled Pac-Man maze game visualization
-- ====================================================================

effect_name = "Pacman"
effect_description = "AI Pac-Man navigating maze with ghosts"

local TILE_SIZE   = 4
local MAZE_W      = 32
local MAZE_H      = 16

-- Compatibility for old/new Lua versions
local unpack = table.unpack or unpack
local atan2  = math.atan2 or function(y,x) return math.atan(y,x) end

---------------------------------------------------------
-- Maze definition
---------------------------------------------------------

local mazeRows = {}
local mazeTemplate = {
    "################################",
    "#............##............#####",
    "#.######.####.##.####.######.###",
    "#o######.####.##.####.######o###",
    "#.######.####.##.####.######.###",
    "#.............................##",
    "####.##.######G#H#I#J#.##.######",
    "####.##....#######....##.#######",
    "####.######.#####.######.#######",
    "####.######.#####.######.#######",
    "#..P.........##............#####",
    "#.######.####.##.####.######.###",
    "#o....#................#....o###",
    "######.#.############.#.########",
    "#......#....######....#......###",
    "################################"
}

local function getTile(tx,ty)
    if tx < 0 or tx >= MAZE_W or ty < 0 or ty >= MAZE_H then
        return "#"
    end
    local row = mazeRows[ty+1]
    return row:sub(tx+1, tx+1)
end

local function isWall(tx,ty)
    return getTile(tx,ty) == "#"
end

---------------------------------------------------------
-- Pellets
---------------------------------------------------------

local pellets     = {}
local powerPellet = {}

local function initPellets()
    pellets     = {}
    powerPellet = {}

    for ty = 0, MAZE_H-1 do
        pellets[ty]     = {}
        powerPellet[ty] = {}

        local row = mazeRows[ty+1]
        for tx = 0, MAZE_W-1 do
            local c = row:sub(tx+1, tx+1)
            pellets[ty][tx]     = (c == "." or c == "o")
            powerPellet[ty][tx] = (c == "o")
        end
    end
end

local function eatPelletAt(tx,ty)
    if pellets[ty] and pellets[ty][tx] then
        local wasPower = powerPellet[ty][tx] or false
        pellets[ty][tx]     = false
        powerPellet[ty][tx] = false
        return wasPower
    end
    return false
end

local function drawMazeWalls()
    local r,g,b = 0,70,255
    for ty = 0, MAZE_H-1 do
        local row = mazeRows[ty+1]
        for tx = 0, MAZE_W-1 do
            if row:sub(tx+1,tx+1) == "#" then
                local x0 = tx*TILE_SIZE
                local y0 = ty*TILE_SIZE
                for yy = 0, TILE_SIZE-1 do
                    for xx = 0, TILE_SIZE-1 do
                        setPixel(x0+xx, y0+yy, r,g,b)
                    end
                end
            end
        end
    end
end

local function drawPellets()
    for ty=0,MAZE_H-1 do
        for tx=0,MAZE_W-1 do
            if pellets[ty][tx] then
                local x = tx*TILE_SIZE + TILE_SIZE/2
                local y = ty*TILE_SIZE + TILE_SIZE/2
                if powerPellet[ty][tx] then
                    setPixel(math.floor(x), math.floor(y), 248,184,0)
                    setPixel(math.floor(x+1), math.floor(y), 248,184,0)
                else
                    setPixel(math.floor(x), math.floor(y), 248,184,0)
                end
            end
        end
    end
end

---------------------------------------------------------
-- Entities
---------------------------------------------------------
local pac = {
    tx = 1, ty = 1,
    x  = 0, y  = 0,
    dx = 1, dy = 0,
    speed = 40,
    mouthPhase = 0,
    radius = 6
}

local ghosts = {}
local scaredTimer = 0
local lastTime = 0

---------------------------------------------------------
-- Draw Pac-Man (4-direction mouth)
---------------------------------------------------------

local function drawPacman(cx,cy,radius,mouthDeg,dx,dy)
    for y = cy-radius, cy+radius do
        for x = cx-radius, cx+radius do
            local lx = x-cx
            local ly = y-cy
            local dist = math.sqrt(lx*lx + ly*ly)
            if dist <= radius then
                local angle = math.deg(atan2(ly, lx))

                if dx== 1 then angle = angle
                elseif dx==-1 then angle = angle + 180
                elseif dy==-1 then angle = angle - 90
                elseif dy== 1 then angle = angle + 90 end

                if angle > 180 then angle = angle - 360 end
                if angle < -180 then angle = angle + 360 end

                local inMouth = math.abs(angle) < mouthDeg/2
                if not inMouth then
                    setPixel(math.floor(x), math.floor(y), 255,255,0)
                end
            end
        end
    end

    -- Eye
    local ex,ey = cx,cy
    if dx==1 then ex,ey = cx,cy-2
    elseif dx==-1 then ex,ey = cx,cy-2
    elseif dy==-1 then ex,ey = cx-2,cy
    elseif dy==1 then ex,ey = cx-2,cy end
    setPixel(math.floor(ex), math.floor(ey), 0,0,0)
end

---------------------------------------------------------
-- Draw Ghost
---------------------------------------------------------
local function drawGhost(cx,cy,radius,col,scared)
    local r,g,b = col[1],col[2],col[3]
    if scared then r,g,b = 0,0,255 end

    for y = cy-radius, cy+radius do
        for x = cx-radius, cx+radius do
            local dx = x - cx
            local dy = y - cy

            if dy <= 0 then
                if math.sqrt(dx*dx + dy*dy) <= radius then
                    setPixel(math.floor(x), math.floor(y), r,g,b)
                end
            else
                if math.abs(dx) <= radius-1 then
                    local wave = math.floor(math.sin((x+cy)*0.5)*2)
                    if dy <= radius + wave then
                        setPixel(math.floor(x), math.floor(y), r,g,b)
                    end
                end
            end
        end
    end

    -- Eyes
    for _,ox in ipairs({-3,3}) do
        setPixel(math.floor(cx+ox), math.floor(cy-2), 255,255,255)
        setPixel(math.floor(cx+ox+1), math.floor(cy-2), 0,0,0)
    end
end

---------------------------------------------------------
-- Parse Maze for Pac-Man & Ghost locations
---------------------------------------------------------
local function parseMaze()
    local ghostColors = {
        G = {255,0,0},
        H = {255,128,255},
        I = {0,255,255},
        J = {255,160,0}
    }

    ghosts = {}

    for ty=0,MAZE_H-1 do
        local row = mazeRows[ty+1]
        local bytes = {row:byte(1,#row)}

        for tx=0,MAZE_W-1 do
            local ch = row:sub(tx+1,tx+1)

            if ch == "P" then
                pac.tx,pac.ty = tx,ty
                pac.x = tx*TILE_SIZE + TILE_SIZE/2
                pac.y = ty*TILE_SIZE + TILE_SIZE/2
                bytes[tx+1] = string.byte(".")

            elseif ghostColors[ch] then
                table.insert(ghosts,{
                    tx = tx, ty = ty,
                    x  = tx*TILE_SIZE + TILE_SIZE/2,
                    y  = ty*TILE_SIZE + TILE_SIZE/2,
                    dx = 0, dy = -1,
                    speed = 30,
                    color = ghostColors[ch]
                })
                bytes[tx+1] = string.byte(".")
            end
        end

        mazeRows[ty+1] = string.char(unpack(bytes))
    end
end

---------------------------------------------------------
-- Movement helpers
---------------------------------------------------------
local dirs = {
    {dx= 1, dy= 0},
    {dx=-1, dy= 0},
    {dx= 0, dy= 1},
    {dx= 0, dy=-1},
}

local function tileCenter(tx,ty)
    return tx*TILE_SIZE + TILE_SIZE/2,
           ty*TILE_SIZE + TILE_SIZE/2
end

local function canMoveTo(tx,ty)
    return not isWall(tx,ty)
end

---------------------------------------------------------
-- Pac-Man movement
---------------------------------------------------------
local function choosePacDirection()
    local candidates={}

    for _,d in ipairs(dirs) do
        local nx,ny = pac.tx+d.dx, pac.ty+d.dy
        if canMoveTo(nx,ny) then
            table.insert(candidates,d)
        end
    end

    if #candidates==0 then return end

    -- Prefer keeping direction if possible
    for _,d in ipairs(candidates) do
        if d.dx==pac.dx and d.dy==pac.dy then
            return
        end
    end

    local chosen = candidates[math.random(#candidates)]
    pac.dx,pac.dy = chosen.dx,chosen.dy
end

local function updatePac(dt)
    local cx,cy = tileCenter(pac.tx,pac.ty)
    local dist = math.abs(pac.x-cx)+math.abs(pac.y-cy)

    if dist < 1 then
        pac.x,pac.y = cx,cy
        choosePacDirection()

        if eatPelletAt(pac.tx,pac.ty) then
            scaredTimer = 6
        end
    end

    pac.x = pac.x + pac.dx * pac.speed * dt
    pac.y = pac.y + pac.dy * pac.speed * dt

    pac.tx = math.floor(pac.x/TILE_SIZE)
    pac.ty = math.floor(pac.y/TILE_SIZE)
end

---------------------------------------------------------
-- Ghost movement
---------------------------------------------------------
local function chooseGhostDirection(g)
    local best, bestScore=nil,1e9

    for _,d in ipairs(dirs) do
        local nx,ny = g.tx+d.dx, g.ty+d.dy
        if canMoveTo(nx,ny) then
            local score = math.abs(nx-pac.tx)+math.abs(ny-pac.ty)
            if d.dx == -g.dx and d.dy == -g.dy then
                score = score + 1
            end
            if score < bestScore then
                bestScore, best = score, d
            end
        end
    end

    if best then
        g.dx,g.dy = best.dx,best.dy
    else
        g.dx,g.dy = -g.dx,-g.dy
    end
end

local function updateGhost(g,dt)
    local cx,cy = tileCenter(g.tx,g.ty)
    local dist = math.abs(g.x-cx)+math.abs(g.y-cy)

    if dist < 1 then
        g.x,g.y = cx,cy
        chooseGhostDirection(g)
    end

    local speedMul = scaredTimer>0 and 0.6 or 1
    g.x = g.x + g.dx*g.speed*speedMul*dt
    g.y = g.y + g.dy*g.speed*speedMul*dt

    g.tx = math.floor(g.x/TILE_SIZE)
    g.ty = math.floor(g.y/TILE_SIZE)
end

---------------------------------------------------------
-- Effect API
---------------------------------------------------------

function init(width, height)
    -- Reset maze from template
    mazeRows = {}
    for i, row in ipairs(mazeTemplate) do
        mazeRows[i] = row
    end

    pac = {
        tx = 1, ty = 1,
        x  = 0, y  = 0,
        dx = 1, dy = 0,
        speed = 40,
        mouthPhase = 0,
        radius = 6
    }
    ghosts = {}
    scaredTimer = 0
    lastTime = 0

    parseMaze()
    initPellets()
end

function reset()
    init(WIDTH, HEIGHT)
end

function update(audio, settings, time)
    local dt = time - lastTime
    if dt <= 0 or dt > 0.1 then dt = 0.016 end
    lastTime = time

    clear()

    pac.speed = 40
    pac.mouthPhase = pac.mouthPhase + dt * 8
    local mouthDeg = 30 + 25*math.abs(math.sin(pac.mouthPhase))

    if scaredTimer > 0 then
        scaredTimer = scaredTimer - dt
        if scaredTimer < 0 then scaredTimer = 0 end
    end
    local scared = (scaredTimer > 0)

    updatePac(dt)
    for _,g in ipairs(ghosts) do updateGhost(g,dt) end

    drawMazeWalls()
    drawPellets()
    drawPacman(pac.x, pac.y, pac.radius, mouthDeg, pac.dx, pac.dy)

    for _,g in ipairs(ghosts) do
        drawGhost(g.x, g.y, 6, g.color, scared)
    end
end
