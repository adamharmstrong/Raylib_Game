# Processed Mercury Assets

`power_pulley_panic_title_theme.wav` is the 16-bar, 140 BPM, G-minor title
theme. It puts the garage beat, Lead 1 hook, Reese bass, and sub on bar one,
then varies their density across a short 27.4-second loop. It uses the Atmo,
Downlifter, Lead 1, Lead 2, Reese, Stab, and Sub stems from `Song Starter 03
140BPM Gmin`, plus Garage Drum Loop 01 and Percussion Loop 04.

`power_pulley_panic_level_select_theme.wav` is the 24-bar, 128 BPM,
E-flat-major theme for the level-select map. It uses Arp Loop 01, HiHat Loop
01, and Percussion Loop 02. An original soft menu beat, warm map bass, pad, and
six-note waypoint hook create an immediate but lower-intensity 45-second loop
for browsing the six campaign nodes.

`power_pulley_panic_level_theme_01.wav` preserves the original 64-second, 120
BPM, G-flat-minor arrangement as a level-theme candidate. It uses the Drone
Atmos, Choir, Keys, Melody Dry, Bass Sub, Bass Top, and Percussion stems in
`Song Starter 01 120BPM Gbm`.

`power_pulley_panic_level_01_gatehouse.wav` is the 32-bar, 135 BPM, G-minor
theme for Level 1, Gatehouse Generator. It uses Chord Loop 01, Garage Drum Loop
03, HiHat Loop 04, short Stab and Growl accents from Song Starter 03, and an
original synthesized G-minor bass motif. Its 56.9-second form moves from a full
machine groove through a restrained middle section and back to a denser finish.

`power_pulley_panic_level_02_rotary_latch_lab.wav` is the 32-bar, 126 BPM,
D-minor theme for Level 2, Rotary Latch Lab. It uses Stab Loop 03, Garage Drum
Loop 02, Percussion Loop 01, and the garage crash stem. Original bass, rotor
drone, and five-note stereo latch motifs reinforce the level's alignment and
five-wheel timing mechanics across its 61-second form.

`power_pulley_panic_level_03_flooded_lower_works.wav` is the 32-bar, 122 BPM,
F-major theme for Level 3, Flooded Lower Works. It uses the Arp, Bass, and Piano
stems from Song Starter 04 with House Drum Loop 02 and Percussion Loop 03.
Original pressure hum and a six-note rising-water motif move across the stereo
field during its 63-second pump, submerged-breakdown, and refill structure.

`power_pulley_panic_level_04_counterweight_row.wav` is the 48-bar, 170 BPM,
C-minor theme for Level 4, Counterweight Row. It uses the non-vocal Song Starter
06 stems with DnB Drum Loop 01. Original descending counterweight impacts and
alternating arrow ticks reinforce its boulder-cover, breakable-floor, and lower
service-lane sequence across a 68-second jungle/DnB form.

`power_pulley_panic_level_05_neurotoxin_annex.wav` is the 32-bar, 137 BPM,
F-minor theme for Level 5, The Neurotoxin Annex. It uses Arp Loop 02, House Drum
Loop 01, and HiHat Loop 05. Original machine bass, three-tone warning signals,
and filtered toxin-breath layers follow the valve climb, airless midpoint, and
urgent return across its 56-second form.

`power_pulley_panic_level_06_clocktower_core.wav` is the 48-bar, 172 BPM,
B-minor finale theme for Level 6, Clocktower Core. It uses the non-vocal Song
Starter 05 stems with DnB Drum Loop 02 and HiHat Loop 02. Original alternating
escapement ticks, four-note belfry hooks, and three rising hand-lock strikes
follow the four-tier ascent and synchronized-clock payoff across its 68-second
form.

All arrangements deliberately omit the vocal stems. Their source automation
and rendering pipelines are in `tools/build_title_theme_02.py` and
`tools/build_title_theme.py`, respectively. The level-select theme is rendered
by `tools/build_level_select_theme.py`. The Gatehouse theme is rendered by
`tools/build_level_01_theme.py`; the Rotary Latch Lab theme is rendered by
`tools/build_level_02_theme.py`; and the Flooded Lower Works theme is rendered
by `tools/build_level_03_theme.py`. The Counterweight Row theme is rendered by
`tools/build_level_04_theme.py`; The Neurotoxin Annex theme is rendered by
`tools/build_level_05_theme.py`; and the Clocktower Core theme is rendered by
`tools/build_level_06_theme.py`.

This derived file remains under the Clark Audio vendor folder to preserve its
third-party provenance. Confirm the Mercury pack's redistribution terms before
shipping the game.
