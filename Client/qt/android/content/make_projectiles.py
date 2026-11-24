#!/usr/bin/env python3
"""Generate original projectile models for the Android client's built-in tools."""

from pathlib import Path
from xml.sax.saxutils import escape


def model(name, size, color, shape, source):
    dimensions = "".join(f"<{axis}>{value}</{axis}>" for axis, value in zip("XYZ", size))
    return (
        '<roblox version="4"><Item class="Part"><Properties>'
        f'<string name="Name">{name}</string><int name="BrickColor">{color}</int>'
        f'<token name="shape">{shape}</token><Vector3 name="size">{dimensions}</Vector3>'
        '<bool name="Anchored">false</bool><bool name="CanCollide">true</bool>'
        '<token name="TopSurface">0</token><token name="BottomSurface">0</token>'
        '</Properties><Item class="Script"><Properties>'
        '<string name="Name">Projectile</string>'
        f'<string name="Source">{escape(source.strip())}</string>'
        '</Properties></Item></Item></roblox>\n'
    )


ROCKET = """
local rocket = script.Parent
local debris = game:GetService("Debris")
debris:AddItem(rocket, 8)

local motor = Instance.new("BodyVelocity")
motor.maxForce = Vector3.new(100000, 100000, 100000)
motor.P = 10000
motor.velocity = rocket.CFrame.lookVector * 85
motor.Parent = rocket
rocket.Velocity = motor.velocity

local exploded = false
rocket.Touched:connect(function(hit)
    if exploded or hit == rocket or hit.Name == "Rocket" then return end
    exploded = true
    local blast = Instance.new("Explosion")
    blast.Position = rocket.Position
    blast.BlastRadius = 8
    blast.BlastPressure = 500000
    blast.Parent = game.Workspace
    debris:AddItem(blast, 2)
    rocket:Remove()
end)
"""

PELLET = """
local pellet = script.Parent
game:GetService("Debris"):AddItem(pellet, 5)
local hitOnce = false
pellet.Touched:connect(function(hit)
    if hitOnce or hit == pellet or hit.Name == "SlingshotPellet" then return end
    hitOnce = true
    local humanoid = hit.Parent and hit.Parent:findFirstChild("Humanoid")
    if humanoid then humanoid:TakeDamage(15) end
    pellet:Remove()
end)
"""


if __name__ == "__main__":
    fonts = Path(__file__).resolve().parent / "fonts"
    fonts.mkdir(exist_ok=True)
    (fonts / "Rocket.rbxm").write_text(model("Rocket", (1, 1, 3), 21, 1, ROCKET))
    (fonts / "SlingshotPellet.rbxm").write_text(model("SlingshotPellet", (1, 1, 1), 199, 0, PELLET))
