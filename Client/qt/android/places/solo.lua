-- Starts a single-player session on the open place: local player, character, physics running.
game:GetService("Visit")
local player = game:GetService("Players"):CreateLocalPlayer(0)
player:LoadCharacter()
game:GetService("RunService"):Run()
print("Solo character loaded")

-- Respawn a few seconds after the character dies, as the website's visit script used to.
while true do
	wait(1)
	local character = player.Character
	local humanoid = character and character:findFirstChild("Humanoid")
	if humanoid == nil or humanoid.Health <= 0 then
		wait(4)
		player:LoadCharacter()
	end
end
