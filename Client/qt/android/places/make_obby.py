#!/usr/bin/env python3
"""Generate obby.rbxl, a small obstacle course for testing character control on Android.

Run: python3 make_obby.py > obby.rbxl
The course climbs from a spawn pad over gapped platforms, stairs and a tower to a gold finish pad,
all above a large baseplate so a missed jump only drops the player back to the ground.
"""
import sys
from xml.sax.saxutils import escape

# BrickColor numbers from the 2007 palette.
WHITE, GREY, DARK_GREY, RED, BLUE, YELLOW, GREEN, DARK_GREEN = 1, 194, 199, 21, 23, 24, 37, 28
ORANGE, PURPLE, BLACK, GOLD = 106, 104, 26, 24

parts = []


def cframe(x, y, z):
    rot = "".join(f"<R{i}{j}>{1 if i == j else 0}</R{i}{j}>" for i in range(3) for j in range(3))
    return f"<X>{x}</X><Y>{y}</Y><Z>{z}</Z>{rot}"


def part(name, color, pos, size, cls="Part", extra=""):
    x, y, z = pos
    sx, sy, sz = size
    parts.append(
        f'<Item class="{cls}"><Properties><string name="Name">{escape(name)}</string>'
        f'<int name="BrickColor">{color}</int><bool name="Anchored">true</bool><token name="shape">1</token>'
        f'<Vector3 name="size"><X>{sx}</X><Y>{sy}</Y><Z>{sz}</Z></Vector3>'
        f'<CoordinateFrame name="CFrame">{cframe(x, y, z)}</CoordinateFrame>{extra}</Properties></Item>'
    )


# Ground: tops at y=0.
part("Baseplate", GREEN, (0, -0.6, 0), (200, 1.2, 200))
part(
    "Spawn",
    GREY,
    (0, 0.5, 40),
    (8, 1, 8),
    cls="SpawnLocation",
    extra='<bool name="Neutral">true</bool>',
)

# 1. Stepping stones going north (-Z), rising slowly.
colors = [RED, BLUE, YELLOW, ORANGE, PURPLE, RED, BLUE]
y = 1.0
z = 30
for i, c in enumerate(colors):
    part(f"Stone{i + 1}", c, (0 if i % 2 == 0 else 3, y, z), (5, 1, 5))
    z -= 8
    y += 1.5

# 2. A long bridge with a pillar to hop over.
bridge_y = y
part("Bridge", DARK_GREY, (0, bridge_y, z - 10), (4, 1, 24))
part("Hurdle", RED, (0, bridge_y + 1.5, z - 10), (4, 2, 1))
z -= 26

# 3. Stairs turning east (+X).
x = 0
y = bridge_y
for i in range(8):
    y += 1
    part(f"Stair{i + 1}", WHITE if i % 2 else GREY, (x, y, z), (4, 1, 4))
    x += 4

# 4. Platforms with growing gaps on the way back south (+Z).
gaps = [5, 6, 7, 7, 8]
z_pos = z
for i, g in enumerate(gaps):
    z_pos += g + 4
    part(f"Gap{i + 1}", [BLUE, YELLOW, PURPLE, ORANGE, RED][i], (x, y, z_pos), (4, 1, 4))

# 5. Tower: a spiral of small blocks around a central column.
cx, cz = x + 12, z_pos
top = y
TOWER_AT = len(parts)
part("Tower", DARK_GREY, (cx, 0, cz), (4, 1, 4))  # resized once the top is known
for i in range(10):
    ang = [(0, -5), (5, 0), (0, 5), (-5, 0)][i % 4]
    top += 2
    part(f"Spiral{i + 1}", [RED, YELLOW, BLUE, WHITE][i % 4], (cx + ang[0], top, cz + ang[1]), (3, 1, 3))
part("Path to tower", GREY, (x + 5, y, z_pos), (6, 1, 3))

# 6. Finish pad on top of the tower.
finish_y = max(top + 2, 26.5)
part("Finish", YELLOW, (cx, finish_y, cz), (8, 1, 8))
tower_h = finish_y - 0.5
parts[TOWER_AT] = parts[TOWER_AT].replace("<Y>0</Y>", f"<Y>{tower_h / 2}</Y>", 1).replace(
    "<Y>1</Y>", f"<Y>{tower_h}</Y>", 1
)

# Scenery: a few blocks on the ground for walking around.
part("Ramp block", ORANGE, (-30, 2, 20), (10, 4, 10))
part("Wall", DARK_GREY, (-30, 4, 0), (2, 8, 30))
part("Low step", WHITE, (-20, 0.5, 20), (6, 1, 6))
part("High step", WHITE, (-24, 1.5, 20), (4, 1, 6))

finish_script = """
local pad = script.Parent
local done = false
pad.Touched:connect(function(hit)
    if done then return end
    local humanoid = hit.Parent:findFirstChild("Humanoid")
    if humanoid == nil then return end
    done = true
    local m = Instance.new("Message")
    m.Text = "You reached the top!"
    m.Parent = game.Workspace
    wait(4)
    m:Remove()
    done = false
end)
"""
# Attach the finish script to the last part.
parts[-5] = parts[-5].replace(
    "</Properties></Item>",
    '</Properties><Item class="Script"><Properties><string name="Name">FinishScript</string>'
    f'<string name="Source">{escape(finish_script)}</string></Properties></Item></Item>',
)

sys.stdout.write(
    '<roblox version="4"><Item class="Workspace"><Properties></Properties>'
    + "".join(parts)
    + '</Item><Item class="StarterPack">'
    + "".join(
        f'<Item class="HopperBin"><Properties><string name="Name">{n}</string><token name="BinType">{t}</token></Properties></Item>'
        for n, t in [("Rocket", 6), ("Slingshot", 5), ("Laser", 7), ("Grab", 2), ("Clone", 3), ("Hammer", 4)]
    )
    + "</Item></roblox>\n"
)
