// PNG to Lua Table Converter
// Compile: csc PngToLua.cs  (or dotnet build)
// Usage: PngToLua.exe image.png [output.lua]

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;

class PngToLua
{
    static void Main(string[] args)
    {
        if (args.Length < 1)
        {
            Console.WriteLine("PNG to Lua Table Converter");
            Console.WriteLine("Usage: PngToLua.exe image.png [output.lua]");
            Console.WriteLine("       PngToLua.exe *.png [output.lua]");
            return;
        }

        var inputFiles = new List<string>();
        string outputFile = null;

        // Parse arguments
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i].Contains("*"))
            {
                // Glob pattern
                string dir = Path.GetDirectoryName(args[i]);
                if (string.IsNullOrEmpty(dir)) dir = ".";
                string pattern = Path.GetFileName(args[i]);
                inputFiles.AddRange(Directory.GetFiles(dir, pattern));
            }
            else if (args[i].EndsWith(".lua", StringComparison.OrdinalIgnoreCase))
            {
                outputFile = args[i];
            }
            else if (File.Exists(args[i]))
            {
                inputFiles.Add(args[i]);
            }
        }

        if (inputFiles.Count == 0)
        {
            Console.WriteLine("Error: No input PNG files found");
            return;
        }

        var sb = new StringBuilder();
        sb.AppendLine("-- Auto-generated sprite data");
        sb.AppendLine($"-- Source: {string.Join(", ", inputFiles.Select(Path.GetFileName))}");
        sb.AppendLine();
        sb.AppendLine(GetDrawFunction());

        foreach (var file in inputFiles)
        {
            try
            {
                string lua = ConvertPng(file);
                sb.AppendLine(lua);
                Console.WriteLine($"Converted: {file}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error with {file}: {ex.Message}");
            }
        }

        string result = sb.ToString();

        if (outputFile != null)
        {
            File.WriteAllText(outputFile, result);
            Console.WriteLine($"\nWritten to: {outputFile}");
        }
        else
        {
            Console.WriteLine("\n" + new string('=', 60));
            Console.WriteLine("LUA OUTPUT:");
            Console.WriteLine(new string('=', 60));
            Console.WriteLine(result);
        }
    }

    static string ConvertPng(string path)
    {
        string name = Path.GetFileNameWithoutExtension(path)
            .Replace("-", "_").Replace(" ", "_");
        if (char.IsDigit(name[0])) name = "sprite_" + name;

        using (var img = new Bitmap(path))
        {
            // Extract palette
            var colorCount = new Dictionary<(int r, int g, int b, bool trans), int>();

            for (int y = 0; y < img.Height; y++)
            {
                for (int x = 0; x < img.Width; x++)
                {
                    Color c = img.GetPixel(x, y);
                    var key = c.A < 128
                        ? (0, 0, 0, true)
                        : (c.R, c.G, c.B, false);

                    if (!colorCount.ContainsKey(key))
                        colorCount[key] = 0;
                    colorCount[key]++;
                }
            }

            // Build palette (transparent first if exists)
            var palette = new List<(int r, int g, int b)>();
            bool hasTransparent = colorCount.Keys.Any(k => k.trans);

            if (hasTransparent)
                palette.Add((0, 0, 0)); // Index 0 = transparent

            foreach (var kv in colorCount.OrderByDescending(x => x.Value))
            {
                if (!kv.Key.trans)
                    palette.Add((kv.Key.r, kv.Key.g, kv.Key.b));
            }

            // Create color to index map
            var colorToIdx = new Dictionary<(int, int, int, bool), int>();
            for (int i = 0; i < palette.Count; i++)
            {
                var p = palette[i];
                bool trans = (i == 0 && hasTransparent);
                colorToIdx[(p.r, p.g, p.b, trans)] = i;
            }

            // Convert to indexed
            var indexed = new int[img.Height, img.Width];
            for (int y = 0; y < img.Height; y++)
            {
                for (int x = 0; x < img.Width; x++)
                {
                    Color c = img.GetPixel(x, y);
                    if (c.A < 128 && hasTransparent)
                    {
                        indexed[y, x] = 0;
                    }
                    else
                    {
                        var key = (c.R, c.G, c.B, false);
                        indexed[y, x] = colorToIdx.ContainsKey(key) ? colorToIdx[key] : 0;
                    }
                }
            }

            // Generate Lua
            var sb = new StringBuilder();

            // Palette
            sb.AppendLine($"local {name}_palette = {{");
            for (int i = 0; i < palette.Count; i++)
            {
                var p = palette[i];
                string comment = (i == 0 && hasTransparent) ? " -- transparent" : "";
                sb.AppendLine($"    {{{p.r,3}, {p.g,3}, {p.b,3}}},{comment}");
            }
            sb.AppendLine("}");
            sb.AppendLine();

            // Dimensions
            sb.AppendLine($"local {name}_width = {img.Width}");
            sb.AppendLine($"local {name}_height = {img.Height}");
            sb.AppendLine();

            // Data
            sb.AppendLine($"local {name} = {{");
            for (int y = 0; y < img.Height; y++)
            {
                var row = new List<string>();
                for (int x = 0; x < img.Width; x++)
                    row.Add(indexed[y, x].ToString().PadLeft(2));
                sb.AppendLine($"    {{{string.Join(",", row)}}},");
            }
            sb.AppendLine("}");
            sb.AppendLine();

            Console.WriteLine($"  Size: {img.Width}x{img.Height}, Colors: {palette.Count}");
            return sb.ToString();
        }
    }

    static string GetDrawFunction()
    {
        return @"-- Draw sprite at position (x, y)
-- Set skip_zero=true to treat palette index 0 as transparent
local function drawSprite(sprite, palette, x, y, skip_zero)
    for row_idx, row in ipairs(sprite) do
        for col_idx, color_idx in ipairs(row) do
            if not skip_zero or color_idx ~= 0 then
                local color = palette[color_idx + 1]  -- Lua is 1-indexed
                if color then
                    local px = x + col_idx - 1
                    local py = y + row_idx - 1
                    if px >= 0 and px < WIDTH and py >= 0 and py < HEIGHT then
                        setPixel(px, py, color[1], color[2], color[3])
                    end
                end
            end
        end
    end
end

-- Draw sprite scaled by factor
local function drawSpriteScaled(sprite, palette, x, y, scale, skip_zero)
    for row_idx, row in ipairs(sprite) do
        for col_idx, color_idx in ipairs(row) do
            if not skip_zero or color_idx ~= 0 then
                local color = palette[color_idx + 1]
                if color then
                    for sy = 0, scale - 1 do
                        for sx = 0, scale - 1 do
                            local px = x + (col_idx - 1) * scale + sx
                            local py = y + (row_idx - 1) * scale + sy
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

";
    }
}
