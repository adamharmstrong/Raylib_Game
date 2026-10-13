-- Attach to a tile or game object. Entering its bounds launches the player.
return {
    on_touch = function(self, player_index)
        local player = self:get_player(player_index)
        if player then
            self:set_player_velocity(player_index, player.vx, -650)
        end
    end
}
