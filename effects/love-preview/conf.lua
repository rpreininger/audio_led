-- LÖVE configuration for LED effect preview
function love.conf(t)
    t.window.title = "LED Effect Preview"
    t.window.width = 1024   -- 128 * 8 scale
    t.window.height = 512   -- 64 * 8 scale
    t.window.resizable = true
    t.console = true        -- Enable console for debugging on Windows
end
