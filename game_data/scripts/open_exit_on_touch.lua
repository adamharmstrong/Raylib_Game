-- Attach to a tile or game object to turn it into an exit-unlocking trigger.
return {
    on_touch = function(self, player_index)
        if not self.opened then
            self.opened = true
            self:open_exit()
            self:log("Exit unlocked by player " .. player_index)
        end
    end
}
