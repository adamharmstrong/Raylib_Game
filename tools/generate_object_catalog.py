from __future__ import annotations

import math
import os
import textwrap
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageDraw
from reportlab.lib.colors import Color, HexColor
from reportlab.lib.pagesizes import landscape, letter
from reportlab.lib.utils import ImageReader
from reportlab.pdfbase.pdfmetrics import stringWidth
from reportlab.pdfgen import canvas


ROOT = Path(__file__).resolve().parents[1]
TMP = ROOT / "tmp" / "pdfs" / "object_catalog_assets"
OUTPUT = ROOT / "output" / "pdf" / "Power_Pulley_Panic_Object_Catalog.pdf"

W, H = 440, 260
BG = (27, 36, 43, 255)
PANEL = (42, 53, 60, 255)
INK = (12, 16, 19, 255)
STEEL = (119, 133, 137, 255)
LIGHT = (205, 216, 211, 255)
ORANGE = (232, 143, 38, 255)
CYAN = (65, 173, 205, 255)
GREEN = (93, 190, 104, 255)
RED = (205, 67, 52, 255)
GOLD = (224, 181, 59, 255)
BROWN = (130, 86, 52, 255)


@dataclass(frozen=True)
class Entry:
    category: str
    name: str
    record: str
    description: str
    kind: str


ENTRIES: list[Entry] = []


def add(category: str, name: str, record: str, description: str, kind: str) -> None:
    ENTRIES.append(Entry(category, name, record, description, kind))


# Actors
add("Actors", "Player", "Runtime actor", "Controllable worker with walking, climbing, swimming, jumping, pushing, interaction, air, and hazard states.", "player")
add("Actors", "Enemy Robot", "enemy", "Patrolling physics-aware enemy that collides with the world, flexible bodies, fluids, buttons, and players.", "robot")

# World and navigation
add("World", "Floor / Solid", "solid", "Primary static collision geometry, rendered from the industrial floor, wall, ceiling, and fill tiles.", "floor")
add("World", "Platform", "platform", "A compact static ledge used for traversal, machinery support, and fluid obstacles.", "platform")
add("World", "Ladder", "ladder", "Climbable vertical route that supports multiple ladder shafts in a single level.", "ladder")
add("World", "Ramp", "ramp", "A fixed-angle inclined surface used by players and round or rectangular physics bodies.", "ramp")
add("World", "One-Way Platform", "oneWayPlatform", "Supports falling bodies from above while allowing movement upward through its underside.", "oneway")
add("World", "Guide Rail", "guideRail", "Static mechanical rail used to frame or constrain moving assemblies.", "rail")
add("World", "Darkness Zone", "darkness", "A rectangular blackout region whose visibility responds to the level's machine power.", "darkness")
add("World", "Level Sign", "label", "Pixel-aligned wall signage used to identify rooms and test stations without affecting collision.", "sign")
add("World", "Exit Door", "exit", "Tracked industrial shutter that raises over time and exposes an outdoor doorway when unlocked.", "door")
add("World", "Outdoor Doorway", "Runtime doorway", "Scenic world beyond an open exit, including sky, distant terrain, and the threshold path.", "outdoor")
add("World", "Checkpoint", "checkpoint", "Updates the player's respawn position when touched and visually changes after activation.", "checkpoint")
add("World", "Breakable Tile", "breakableTile", "Floor tile that cracks under a player, fails after a delay, and spawns falling fragments.", "breaktile")
add("World", "Breakable Debris", "Runtime debris", "Short-lived fragments emitted by a broken tile; affected by gravity, wind, and fluids.", "debris")
add("World", "Spike Hazard", "spikeHazard", "Static row of sharpened metal spikes that immediately defeats a touching player.", "spikes")

# Classic machines
add("Classic Machines", "Winch", "Runtime machine", "Player-operated carriage that turns the factory pulley network and converts movement into machine power.", "winch")
add("Classic Machines", "Pulley", "pulley", "Grooved wheel used by rope routes, counterweights, factory drives, and compound pulley layouts.", "pulley")
add("Classic Machines", "Hanging Weight", "weight", "Counterweight suspended from a pulley; oscillates with machine phase and acts as a hazard.", "weight")
add("Classic Machines", "Rotary Latch", "rotaryLatch", "Spinning wheel lock that must be aligned and secured to complete a gate circuit.", "latch")
add("Classic Machines", "Valve", "valve", "Interactable hand wheel that controls water filling or shuts off a neurotoxin leak.", "valve")
add("Classic Machines", "Flood Pump", "Runtime pump", "Pipe, outlet, and pump assembly that visualizes valve-driven water delivery.", "pump")
add("Classic Machines", "Stone Block", "stoneBlock", "Heavy pushable block with high friction, low bounce, fluid response, and button weight.", "stone")
add("Classic Machines", "Boulder", "boulder", "Massive rolling stone with angular momentum, ramp response, and projectile shielding.", "boulder")
add("Classic Machines", "Physics Wheel", "physicsWheel", "Loose wheel with lower rolling resistance and stronger rotational coupling than a boulder.", "wheel")
add("Classic Machines", "Gear", "gear", "Dynamic toothed wheel that rolls, meshes with screw grooves, and carries angular velocity.", "gear")
add("Classic Machines", "Flywheel", "flywheel", "Heavy rimmed wheel that stores rotational energy and resists abrupt speed changes.", "flywheel")
add("Classic Machines", "Steering Wheel", "steeringWheel", "Hand wheel intended for valves and mechanisms that need direct player turning.", "steering")
add("Classic Machines", "Screw", "screw", "Rotating helical shaft that conveys contacting bodies and mechanically couples to a matching gear.", "screw")
add("Classic Machines", "Fan", "fan", "Powered rotor that emits directional wind with configurable range, width, strength, and power.", "fan")
add("Classic Machines", "Wind Stream", "Runtime force field", "Visible streaks and a directional force volume generated by a fan.", "wind")
add("Classic Machines", "Pinwheel", "pinwheel", "Light rotor whose spin rate responds directly to local wind velocity.", "pinwheel")
add("Classic Machines", "See-Saw", "seeSaw", "Hinged balance beam that tilts from player and object mass around a central fulcrum.", "seesaw")
add("Classic Machines", "Trap Door", "trapDoor", "One-sided hinged floor door with two external rings for ropes and chains.", "trapdoor")
add("Classic Machines", "Chain", "chain", "Alternating front and side links simulated as a flexible collision body with detachable endpoints.", "chain")
add("Classic Machines", "Physics Rope", "physicsRope", "Segmented rope with gravity, collision, fluid response, and player-controlled endpoint attachment.", "rope")
add("Classic Machines", "Button", "button", "Pressure plate activated by players, enemies, flexible bodies, and movable physics objects.", "button")
add("Classic Machines", "Arrow Trap", "arrowTrap", "Timed launcher that emits arrows along a cardinal direction at a configured speed.", "arrowtrap")
add("Classic Machines", "Arrow Projectile", "Runtime projectile", "Wind- and fluid-responsive projectile that stops on solids and defeats players on contact.", "arrow")

# Reusable physics guide objects
add("Physics Objects", "Ball", "ball", "Light, bouncy circular rigid body with rolling motion and mass-aware collisions.", "ball")
add("Physics Objects", "Barrel", "barrel", "Pushable dynamic barrel with moderate bounce, friction, and full rigid-body interactions.", "barrel")
add("Physics Objects", "Moving Platform", "movingPlatform", "Kinematic platform that travels along a configurable axis and carries standing players.", "moving")
add("Physics Objects", "Elevator", "elevator", "Vertical kinematic platform with a configurable travel distance and speed.", "elevator")
add("Physics Objects", "Pendulum Bob", "pendulumBob", "Mass suspended from a fixed pivot and animated along a pendulum arc.", "pendulum")
add("Physics Objects", "Ceiling Hook", "ceilingHook", "Fixed overhead attachment point for flexible bodies and hanging mechanisms.", "hook")
add("Physics Objects", "Spring", "spring", "Elastic constraint that pulls an attached body toward its rest length with damping.", "spring")
add("Physics Objects", "Rod", "rod", "Rigid distance constraint that preserves length and removes radial velocity.", "rod")
add("Physics Objects", "Fixed Joint", "fixedJoint", "Locks a nearby loose body to a fixed world-space attachment point.", "fixed")
add("Physics Objects", "Crank", "crank", "Rotating wheel with an offset handle for converting circular input into mechanical motion.", "crank")
add("Physics Objects", "Ratchet", "ratchet", "One-direction rotary mechanism with a visible pawl and nonnegative rotation.", "ratchet")
add("Physics Objects", "Clutch", "clutch", "Engageable rotary coupling that can transmit or interrupt rotation.", "clutch")
add("Physics Objects", "Brake", "brake", "Friction volume that rapidly damps the velocity of overlapping dynamic bodies.", "brake")
add("Physics Objects", "Cam", "cam", "Eccentric rotating lobe used to create repeating offset motion.", "cam")
add("Physics Objects", "Conveyor Belt", "conveyorBelt", "Animated moving surface that transports players and influences their velocity.", "conveyor")
add("Physics Objects", "Turntable", "turntable", "Rotating horizontal disk that applies tangential motion to contacting players.", "turntable")

# Power and sensing
add("Power and Sensors", "Battery", "battery", "Steady power source that energizes every compatible object on its channel.", "battery")
add("Power and Sensors", "Electric Motor", "electricMotor", "Powered rotor that converts channel power into continuous angular motion.", "motor")
add("Power and Sensors", "Limit Switch", "limitSwitch", "Contact sensor with a mechanical lever that energizes its channel when triggered.", "limitswitch")
add("Power and Sensors", "Relay", "relay", "Channel-controlled electrical switching block used to pass a power signal.", "relay")
add("Power and Sensors", "Fuse", "fuse", "Protective link that breaks when supplied power exceeds its configured capacity.", "fuse")
add("Power and Sensors", "Speed Sensor", "speedSensor", "Trigger volume that activates only when a player or body exceeds a minimum speed.", "speedsensor")
add("Power and Sensors", "Beam Sensor", "beamSensor", "Directional optical beam that detects players and loose bodies crossing its path.", "beamsensor")

# Forces and powered motion
add("Forces and Motion", "Magnet", "magnet", "Radial force source that attracts nearby players and loose bodies when powered.", "magnet")
add("Forces and Motion", "Piston", "piston", "Powered kinematic ram that repeatedly extends along a configured direction.", "piston")
add("Forces and Motion", "Hydraulic Cylinder", "hydraulicCylinder", "Slower high-power linear actuator with a visible cylinder and extending rod.", "hydraulic")
add("Forces and Motion", "Rocket Thruster", "rocketThruster", "Directional exhaust volume that accelerates players and dynamic objects.", "rocket")
add("Forces and Motion", "Oil", "oil", "Slippery floor volume that preserves horizontal momentum and reduces traction.", "oil")
add("Forces and Motion", "Mud", "mud", "Viscous floor volume that strongly damps horizontal and vertical movement.", "mud")

# Hazards
add("Hazards", "Crushing Block", "crushingBlock", "Kinematic spiked block that travels along a path and defeats players on contact.", "crusher")
add("Hazards", "Swinging Hammer", "swingingHammer", "Heavy pendulum hazard with a thick handle and damaging hammer head.", "hammer")
add("Hazards", "Saw Blade", "sawBlade", "Fast rotating toothed hazard that can also patrol along a configured path.", "saw")
add("Hazards", "Steam Vent", "steamVent", "Periodic directional jet that visibly pushes players and loose objects.", "steam")
add("Hazards", "Electrical Arc", "electricalArc", "Intermittent powered lightning connection that damages players crossing the beam.", "arc")

# Pickups and destructibles
add("Gameplay Items", "Collectible", "collectible", "Touch pickup represented by a bright token; disappears once collected.", "collectible")
add("Gameplay Items", "Key", "key", "Golden key pickup reserved for locks, gates, and future keyed mechanisms.", "key")
add("Gameplay Items", "Breakable Crate", "breakableCrate", "Dynamic wooden crate that breaks after a sufficiently hard impact.", "crate")
add("Gameplay Items", "Explosive Barrel", "explosiveBarrel", "Red dynamic barrel that detonates after a severe impact and creates a brief blast hazard.", "explosive")

# Fluids and special fields
add("Fluids", "Water", "water", "Cellular fluid with pressure, leveling, splashes, buoyancy, drag, and object displacement.", "water")
add("Fluids", "Sand", "sand", "Granular cellular material with gravity, diagonal flow, friction, and surface avalanching.", "sand")
add("Fluids", "Gel", "gel", "Particle fluid with persistent bonds that stretches, wobbles, and attempts to recover its shape.", "gel")
add("Fluids", "Gas", "gas", "Buoyant particle fluid that diffuses through rooms, responds to fans, and can be vented through doors.", "gas")
add("Fluids", "Water Pit", "waterPit", "Legacy rectangular water reservoir with a controllable surface height and fill target.", "waterpit")
add("Fluids", "Neurotoxin Leak", "toxinLeak", "Valve-controlled gas emitter with animated pipework, exposure, air depletion, and door exhaust.", "toxin")


CATEGORY_COLORS = {
    "Actors": "#57B7D4",
    "World": "#8EA6A5",
    "Classic Machines": "#E38A27",
    "Physics Objects": "#D4A64A",
    "Power and Sensors": "#65B875",
    "Forces and Motion": "#5BA9D1",
    "Hazards": "#D35A47",
    "Gameplay Items": "#D9B94C",
    "Fluids": "#4CA6C9",
}


def line(draw: ImageDraw.ImageDraw, points, fill=LIGHT, width=5) -> None:
    draw.line(points, fill=fill, width=width, joint="curve")


def wheel(draw: ImageDraw.ImageDraw, center=(220, 130), radius=72, spokes=6, rim=STEEL, teeth=0) -> None:
    cx, cy = center
    if teeth:
        for i in range(teeth):
            a = i * math.tau / teeth
            x = cx + math.cos(a) * (radius + 9)
            y = cy + math.sin(a) * (radius + 9)
            draw.rectangle((x - 7, y - 7, x + 7, y + 7), fill=rim, outline=INK, width=3)
    draw.ellipse((cx - radius, cy - radius, cx + radius, cy + radius), fill=rim, outline=INK, width=7)
    draw.ellipse((cx - radius + 17, cy - radius + 17, cx + radius - 17, cy + radius - 17), fill=PANEL, outline=INK, width=5)
    for i in range(spokes):
        a = i * math.tau / spokes
        line(draw, (center, (cx + math.cos(a) * (radius - 20), cy + math.sin(a) * (radius - 20))), rim, 8)
    draw.ellipse((cx - 15, cy - 15, cx + 15, cy + 15), fill=ORANGE, outline=INK, width=4)


def tile_strip(draw: ImageDraw.ImageDraw, y=130, height=55, width=360) -> None:
    x0 = (W - width) // 2
    draw.rectangle((x0, y, x0 + width, y + height), fill=(34, 42, 49), outline=INK, width=6)
    for x in range(x0 + 8, x0 + width - 8, 34):
        draw.rectangle((x, y + 8, min(x + 28, x0 + width - 8), y + 22), fill=(63, 76, 85), outline=(15, 20, 24), width=2)
        draw.line((x, y + 32, min(x + 26, x0 + width - 8), y + 32), fill=(76, 92, 101), width=3)


def paste_asset(canvas: Image.Image, path: Path, box: tuple[int, int, int, int], crop=None) -> bool:
    if not path.exists():
        return False
    image = Image.open(path).convert("RGBA")
    if crop is not None:
        image = image.crop(crop)
    max_w = box[2] - box[0]
    max_h = box[3] - box[1]
    scale = max(1, int(min(max_w / image.width, max_h / image.height)))
    image = image.resize((image.width * scale, image.height * scale), Image.Resampling.NEAREST)
    x = box[0] + (max_w - image.width) // 2
    y = box[1] + (max_h - image.height) // 2
    canvas.alpha_composite(image, (x, y))
    return True


def draw_icon(entry: Entry, output: Path) -> None:
    image = Image.new("RGBA", (W, H), BG)
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((10, 10, W - 10, H - 10), radius=12, fill=PANEL, outline=(75, 92, 101), width=4)
    draw.line((24, 33, W - 24, 33), fill=tuple(int(CATEGORY_COLORS[entry.category][i:i+2], 16) for i in (1, 3, 5)) + (255,), width=5)
    kind = entry.kind

    if kind == "player":
        path = ROOT / "assets" / "first_party" / "characters" / "Player_Sprites.png"
        if not paste_asset(image, path, (130, 48, 310, 238), (37, 0, 74, 47)):
            draw.rectangle((185, 95, 255, 220), fill=ORANGE, outline=INK, width=5)
    elif kind == "robot":
        paths = list((ROOT / "assets" / "third_party").rglob("Anim_Robot_Walk1_v1.1_spritesheet.png"))
        if paths:
            src = Image.open(paths[0])
            fw, fh = src.width // 3, src.height // 2
            paste_asset(image, paths[0], (130, 48, 310, 238), (0, 0, fw, fh))
        else:
            draw.rectangle((170, 80, 270, 210), fill=STEEL, outline=INK, width=6)
    elif kind in {"floor", "platform", "rail", "moving", "oneway", "conveyor"}:
        y = 150 if kind != "moving" else 125
        tile_strip(draw, y, 48, 330 if kind != "platform" else 240)
        if kind == "oneway":
            for x in range(100, 341, 45):
                draw.polygon(((x, 145), (x + 12, 132), (x + 24, 145)), fill=CYAN)
        elif kind == "rail":
            line(draw, ((75, 123), (365, 123)), STEEL, 12)
            for x in range(95, 360, 48):
                draw.ellipse((x - 7, 116, x + 7, 130), fill=ORANGE, outline=INK, width=2)
        elif kind == "moving":
            line(draw, ((120, 210), (320, 210)), CYAN, 5)
            draw.polygon(((115, 210), (138, 196), (138, 224)), fill=CYAN)
            draw.polygon(((325, 210), (302, 196), (302, 224)), fill=CYAN)
        elif kind == "conveyor":
            for x in range(75, 365, 42):
                draw.ellipse((x, 151, x + 42, 193), outline=STEEL, width=4)
    elif kind == "ladder":
        line(draw, ((155, 48), (155, 225)), STEEL, 12)
        line(draw, ((285, 48), (285, 225)), STEEL, 12)
        for y in range(62, 220, 30):
            line(draw, ((155, y), (285, y)), LIGHT, 7)
    elif kind == "ramp":
        draw.polygon(((65, 205), (370, 88), (385, 130), (80, 230)), fill=BROWN, outline=INK)
        line(draw, ((65, 205), (370, 88)), (205, 155, 84), 7)
    elif kind == "darkness":
        draw.rectangle((60, 58, 380, 220), fill=(3, 6, 8), outline=(91, 104, 108), width=5)
        draw.ellipse((165, 88, 275, 198), fill=(20, 30, 35), outline=(54, 70, 76), width=3)
    elif kind == "sign":
        draw.rectangle((70, 75, 370, 200), fill=(216, 208, 170), outline=INK, width=8)
        for y, width in ((105, 230), (135, 270), (165, 180)):
            draw.rectangle((95, y, 95 + width, y + 10), fill=(65, 69, 62))
    elif kind in {"door", "outdoor"}:
        draw.rectangle((145, 48, 295, 228), fill=(49, 58, 63), outline=INK, width=8)
        if kind == "outdoor":
            draw.rectangle((165, 68, 275, 210), fill=(83, 161, 201))
            draw.ellipse((220, 82, 270, 132), fill=GOLD)
            draw.polygon(((165, 180), (215, 130), (275, 185)), fill=(74, 125, 76))
        else:
            for x in range(160, 288, 20):
                draw.rectangle((x, 72, x + 14, 218), fill=STEEL, outline=INK, width=2)
            draw.rectangle((150, 48, 290, 78), fill=(39, 47, 50), outline=INK, width=4)
    elif kind == "checkpoint":
        line(draw, ((150, 55), (150, 225)), LIGHT, 10)
        draw.polygon(((155, 60), (325, 100), (155, 140)), fill=GREEN, outline=INK)
    elif kind in {"breaktile", "crate", "stone"}:
        box = (105, 75, 335, 215)
        fill = BROWN if kind == "crate" else ((105, 108, 103) if kind == "stone" else (55, 67, 74))
        draw.rectangle(box, fill=fill, outline=INK, width=8)
        if kind == "crate":
            line(draw, ((115, 85), (325, 205)), (83, 50, 30), 9)
            line(draw, ((325, 85), (115, 205)), (83, 50, 30), 9)
        elif kind == "breaktile":
            line(draw, ((220, 78), (205, 120), (238, 150), (190, 212)), ORANGE, 5)
        else:
            draw.ellipse((150, 110, 190, 145), fill=(137, 139, 132), outline=INK, width=3)
    elif kind == "debris":
        for i, box in enumerate(((90, 90, 145, 135), (175, 65, 230, 120), (260, 105, 325, 160), (160, 165, 215, 215), (285, 175, 330, 220))):
            draw.polygon(((box[0], box[1]), (box[2], box[1] + 8), (box[2] - 7, box[3]), (box[0] + 8, box[3] - 5)), fill=(74, 85, 91), outline=INK)
    elif kind == "spikes":
        tile_strip(draw, 190, 32, 350)
        for x in range(55, 385, 32):
            draw.polygon(((x, 190), (x + 16, 90), (x + 32, 190)), fill=STEEL, outline=INK)
    elif kind == "winch":
        draw.rectangle((105, 92, 280, 190), fill=STEEL, outline=INK, width=7)
        line(draw, ((120, 105), (265, 177)), INK, 7)
        line(draw, ((265, 105), (120, 177)), INK, 7)
        line(draw, ((280, 140), (365, 140)), BROWN, 8)
        draw.ellipse((330, 105, 400, 175), fill=ORANGE, outline=INK, width=6)
    elif kind in {"pulley", "latch", "valve", "wheel", "gear", "flywheel", "steering", "pinwheel", "crank", "ratchet", "clutch", "motor"}:
        spokes = 4 if kind in {"pinwheel", "crank"} else (8 if kind in {"gear", "ratchet"} else 6)
        teeth = 14 if kind in {"gear", "ratchet"} else 0
        rim = ORANGE if kind in {"pulley", "valve", "crank"} else (GREEN if kind in {"clutch", "motor"} else STEEL)
        wheel(draw, spokes=spokes, rim=rim, teeth=teeth)
        if kind == "latch":
            line(draw, ((220, 130), (280, 80)), GREEN, 9)
        elif kind == "steering":
            draw.ellipse((185, 95, 255, 165), fill=PANEL, outline=LIGHT, width=6)
        elif kind == "pinwheel":
            line(draw, ((220, 130), (220, 230)), LIGHT, 7)
        elif kind == "crank":
            line(draw, ((220, 130), (325, 80)), LIGHT, 10)
            draw.ellipse((310, 65, 342, 97), fill=BROWN, outline=INK, width=4)
        elif kind == "ratchet":
            draw.polygon(((220, 45), (270, 55), (232, 85)), fill=ORANGE, outline=INK)
    elif kind in {"weight", "elevator"}:
        line(draw, ((220, 35), (220, 95)), BROWN if kind == "weight" else STEEL, 8)
        draw.rectangle((150, 95, 290, 220), fill=STEEL, outline=INK, width=8)
        if kind == "elevator":
            line(draw, ((110, 40), (110, 225)), LIGHT, 7)
            line(draw, ((330, 40), (330, 225)), LIGHT, 7)
    elif kind in {"pump", "piston", "hydraulic"}:
        color = CYAN if kind == "hydraulic" else STEEL
        draw.rectangle((70, 95, 240, 180), fill=color, outline=INK, width=8)
        draw.rectangle((240, 117, 365, 158), fill=LIGHT, outline=INK, width=5)
        draw.rectangle((350, 98, 395, 178), fill=ORANGE, outline=INK, width=6)
        if kind == "pump":
            line(draw, ((110, 95), (110, 55), (320, 55), (320, 115)), CYAN, 12)
    elif kind == "boulder":
        draw.ellipse((120, 35, 320, 235), fill=(105, 107, 103), outline=INK, width=8)
        draw.ellipse((165, 75, 220, 120), fill=(137, 139, 132), outline=INK, width=3)
        draw.ellipse((235, 145, 275, 178), fill=(79, 82, 80))
    elif kind == "screw":
        line(draw, ((65, 130), (365, 130)), STEEL, 22)
        for x in range(85, 350, 38):
            line(draw, ((x - 15, 90), (x + 15, 170)), LIGHT, 8)
        draw.ellipse((45, 85, 105, 175), fill=ORANGE, outline=INK, width=6)
    elif kind in {"fan", "wind"}:
        if kind == "fan":
            draw.rectangle((75, 60, 230, 215), fill=(53, 63, 68), outline=INK, width=7)
            wheel(draw, center=(152, 137), radius=60, spokes=5, rim=STEEL)
        for y in (85, 125, 165, 205):
            line(draw, ((235 if kind == "fan" else 75, y), (390, y - 10)), CYAN, 5)
            draw.polygon(((390, y - 10), (370, y - 22), (374, y + 2)), fill=CYAN)
    elif kind in {"seesaw", "trapdoor"}:
        if kind == "seesaw":
            draw.polygon(((190, 215), (250, 215), (220, 145)), fill=STEEL, outline=INK)
            line(draw, ((65, 175), (375, 110)), BROWN, 20)
        else:
            line(draw, ((80, 155), (355, 155)), BROWN, 25)
            draw.ellipse((55, 130, 105, 180), fill=STEEL, outline=INK, width=5)
            draw.ellipse((320, 115, 345, 145), outline=LIGHT, width=5)
            draw.ellipse((320, 165, 345, 195), outline=LIGHT, width=5)
    elif kind in {"chain", "rope"}:
        if kind == "chain":
            for i in range(7):
                x = 80 + i * 45
                box = (x, 85 if i % 2 == 0 else 105, x + 60, 165 if i % 2 == 0 else 145)
                draw.ellipse(box, outline=LIGHT if i % 2 else STEEL, width=9)
        else:
            points = [(60, 85), (115, 120), (170, 105), (230, 155), (295, 125), (380, 175)]
            line(draw, points, BROWN, 12)
            for x, y in points:
                draw.ellipse((x - 6, y - 6, x + 6, y + 6), fill=LIGHT, outline=INK)
    elif kind == "button":
        draw.rectangle((105, 155, 335, 205), fill=(55, 64, 68), outline=INK, width=8)
        draw.rounded_rectangle((135, 105, 305, 170), radius=10, fill=RED, outline=INK, width=7)
    elif kind in {"arrowtrap", "arrow"}:
        if kind == "arrowtrap":
            draw.rectangle((75, 72, 210, 195), fill=STEEL, outline=INK, width=8)
            draw.ellipse((155, 102, 235, 165), fill=(20, 26, 29), outline=INK, width=5)
            start = 200
        else:
            start = 70
        line(draw, ((start, 133), (365, 133)), LIGHT, 9)
        draw.polygon(((380, 133), (335, 105), (335, 161)), fill=ORANGE, outline=INK)
    elif kind == "ball":
        draw.ellipse((125, 35, 315, 225), fill=RED, outline=INK, width=8)
        line(draw, ((220, 130), (295, 75)), LIGHT, 6)
    elif kind in {"barrel", "explosive"}:
        color = RED if kind == "explosive" else BROWN
        draw.rounded_rectangle((150, 45, 290, 225), radius=24, fill=color, outline=INK, width=8)
        for y in (70, 195):
            line(draw, ((152, y), (288, y)), STEEL, 10)
        if kind == "explosive":
            draw.polygon(((220, 90), (185, 165), (255, 165)), fill=GOLD, outline=INK)
    elif kind == "pendulum":
        draw.ellipse((205, 35, 235, 65), fill=STEEL, outline=INK, width=4)
        line(draw, ((220, 60), (305, 180)), BROWN, 9)
        draw.ellipse((250, 145, 360, 245), fill=STEEL, outline=INK, width=7)
    elif kind == "hook":
        line(draw, ((220, 35), (220, 120)), STEEL, 14)
        draw.arc((145, 90, 295, 230), 300, 135, fill=LIGHT, width=16)
    elif kind == "spring":
        points = [(55, 130)]
        for i in range(1, 13):
            points.append((55 + i * 27, 95 if i % 2 else 165))
        points.append((385, 130))
        line(draw, points, LIGHT, 9)
    elif kind == "rod":
        line(draw, ((75, 180), (365, 80)), STEEL, 18)
        for p in ((75, 180), (365, 80)):
            draw.ellipse((p[0] - 18, p[1] - 18, p[0] + 18, p[1] + 18), fill=ORANGE, outline=INK, width=5)
    elif kind == "fixed":
        draw.rectangle((160, 70, 280, 190), fill=STEEL, outline=INK, width=8)
        for p in ((185, 95), (255, 95), (185, 165), (255, 165)):
            draw.ellipse((p[0] - 8, p[1] - 8, p[0] + 8, p[1] + 8), fill=ORANGE, outline=INK, width=3)
    elif kind in {"brake", "relay", "fuse", "speedsensor", "limitswitch"}:
        color = RED if kind == "brake" else (GREEN if kind in {"relay", "speedsensor"} else STEEL)
        draw.rectangle((115, 75, 325, 205), fill=color, outline=INK, width=8)
        if kind == "fuse":
            line(draw, ((150, 140), (290, 140)), ORANGE, 10)
        elif kind == "limitswitch":
            line(draw, ((220, 130), (350, 65)), LIGHT, 8)
            draw.ellipse((335, 48, 370, 83), fill=ORANGE, outline=INK, width=4)
        elif kind == "speedsensor":
            draw.polygon(((175, 95), (285, 140), (175, 185)), fill=CYAN, outline=INK)
        else:
            for x in (155, 220, 285):
                draw.rectangle((x - 12, 105, x + 12, 175), fill=PANEL, outline=INK, width=3)
    elif kind == "cam":
        draw.ellipse((120, 55, 320, 220), fill=STEEL, outline=INK, width=8)
        draw.ellipse((190, 115, 235, 160), fill=ORANGE, outline=INK, width=5)
    elif kind == "turntable":
        draw.ellipse((70, 80, 370, 210), fill=STEEL, outline=INK, width=8)
        line(draw, ((220, 145), (350, 145)), ORANGE, 8)
    elif kind == "battery":
        draw.rectangle((110, 75, 330, 205), fill=(52, 104, 76), outline=INK, width=8)
        draw.rectangle((145, 55, 180, 78), fill=LIGHT, outline=INK, width=3)
        draw.rectangle((265, 55, 300, 78), fill=LIGHT, outline=INK, width=3)
        line(draw, ((160, 105), (160, 165)), GOLD, 9)
        line(draw, ((130, 135), (190, 135)), GOLD, 9)
        line(draw, ((255, 135), (315, 135)), LIGHT, 9)
    elif kind == "beamsensor":
        draw.rectangle((65, 90, 120, 175), fill=STEEL, outline=INK, width=6)
        line(draw, ((120, 132), (380, 132)), RED, 5)
        draw.ellipse((360, 112, 400, 152), fill=GREEN, outline=INK, width=4)
    elif kind == "magnet":
        draw.arc((105, 45, 335, 235), 30, 150, fill=RED, width=38)
        draw.arc((105, 45, 335, 235), 210, 330, fill=CYAN, width=38)
        draw.ellipse((95, 35, 345, 245), outline=(84, 145, 164), width=3)
    elif kind == "rocket":
        draw.polygon(((90, 75), (300, 75), (360, 130), (300, 185), (90, 185)), fill=STEEL, outline=INK)
        for y, color in ((100, GOLD), (130, ORANGE), (160, RED)):
            draw.polygon(((90, y), (25, y - 18), (25, y + 18)), fill=color)
    elif kind == "crusher":
        draw.rectangle((120, 60, 320, 190), fill=(75, 83, 87), outline=INK, width=8)
        for x in range(125, 320, 35):
            draw.polygon(((x, 190), (x + 17, 235), (x + 34, 190)), fill=LIGHT, outline=INK)
    elif kind == "hammer":
        line(draw, ((105, 55), (290, 170)), BROWN, 18)
        draw.rounded_rectangle((245, 125, 380, 220), radius=15, fill=STEEL, outline=INK, width=8)
    elif kind == "saw":
        cx, cy, radius = 220, 135, 85
        for i in range(18):
            a = i * math.tau / 18
            b = a + math.tau / 36
            c = a + math.tau / 18
            draw.polygon(((cx + math.cos(a) * radius, cy + math.sin(a) * radius),
                          (cx + math.cos(b) * (radius + 25), cy + math.sin(b) * (radius + 25)),
                          (cx + math.cos(c) * radius, cy + math.sin(c) * radius)), fill=LIGHT, outline=INK)
        draw.ellipse((cx - radius, cy - radius, cx + radius, cy + radius), fill=STEEL, outline=INK, width=7)
        draw.ellipse((cx - 18, cy - 18, cx + 18, cy + 18), fill=ORANGE, outline=INK, width=4)
    elif kind == "steam":
        draw.rectangle((160, 175, 280, 220), fill=STEEL, outline=INK, width=7)
        for x in (180, 220, 260):
            points = [(x, 175), (x - 22, 135), (x + 18, 95), (x - 5, 48)]
            line(draw, points, (225, 230, 225), 10)
    elif kind == "arc":
        points = [(45, 145), (95, 90), (145, 160), (205, 75), (265, 150), (325, 85), (395, 135)]
        line(draw, points, CYAN, 12)
        line(draw, points, LIGHT, 4)
    elif kind in {"oil", "mud", "water", "sand", "gel", "gas", "waterpit", "toxin"}:
        colors = {
            "oil": (37, 30, 44), "mud": (91, 70, 48), "water": (45, 141, 205),
            "sand": (202, 165, 73), "gel": (65, 123, 203), "gas": (115, 145, 104),
            "waterpit": (45, 141, 205), "toxin": (96, 194, 82),
        }
        color = colors[kind]
        draw.rectangle((55, 105, 385, 220), fill=color, outline=INK, width=7)
        if kind in {"water", "waterpit"}:
            points = [(55, 105), (105, 92), (155, 108), (205, 95), (255, 109), (315, 91), (385, 105)]
            line(draw, points, (212, 239, 244), 6)
        elif kind == "sand":
            for x in range(75, 375, 24):
                for y in range(125, 215, 22):
                    draw.ellipse((x, y, x + 7, y + 7), fill=(235, 208, 118))
        elif kind == "gel":
            for x, y in ((100, 140), (150, 175), (205, 135), (260, 180), (325, 145)):
                draw.ellipse((x - 18, y - 18, x + 18, y + 18), fill=(102, 165, 232), outline=LIGHT, width=3)
                line(draw, ((x, y), (220, 170)), (125, 184, 238), 3)
        elif kind in {"gas", "toxin"}:
            for i in range(28):
                x = 65 + (i * 47) % 310
                y = 55 + (i * 31) % 155
                draw.ellipse((x, y, x + 18, y + 18), fill=color + (145,), outline=None)
            if kind == "toxin":
                line(draw, ((45, 180), (105, 180), (105, 75), (190, 75)), STEEL, 15)
                wheel(draw, center=(220, 75), radius=38, spokes=6, rim=ORANGE)
        else:
            for x in range(70, 380, 35):
                draw.ellipse((x, 115 + (x % 3) * 18, x + 20, 135 + (x % 3) * 18), fill=(65, 55, 72))
    elif kind == "collectible":
        draw.ellipse((135, 45, 305, 215), fill=GOLD, outline=INK, width=8)
        draw.ellipse((180, 90, 260, 170), fill=(247, 220, 83), outline=ORANGE, width=5)
    elif kind == "key":
        draw.ellipse((75, 75, 190, 190), outline=GOLD, width=20)
        line(draw, ((180, 135), (365, 135)), GOLD, 22)
        line(draw, ((285, 135), (285, 185)), GOLD, 18)
        line(draw, ((335, 135), (335, 170)), GOLD, 18)
    else:
        draw.rectangle((110, 65, 330, 210), fill=STEEL, outline=INK, width=8)

    output.parent.mkdir(parents=True, exist_ok=True)
    image.convert("RGB").save(output, quality=95)


def wrap_lines(text: str, font: str, size: float, width: float) -> list[str]:
    words = text.split()
    lines: list[str] = []
    current = ""
    for word in words:
        candidate = word if not current else current + " " + word
        if stringWidth(candidate, font, size) <= width:
            current = candidate
        else:
            if current:
                lines.append(current)
            current = word
    if current:
        lines.append(current)
    return lines


def draw_footer(pdf: canvas.Canvas, page_number: int) -> None:
    pdf.setStrokeColor(HexColor("#33434B"))
    pdf.line(32, 24, 760, 24)
    pdf.setFillColor(HexColor("#788B92"))
    pdf.setFont("Helvetica", 7.5)
    pdf.drawString(34, 12, "POWER PULLEY PANIC - OBJECT CATALOG")
    pdf.drawRightString(758, 12, f"PAGE {page_number}")


def draw_card(pdf: canvas.Canvas, entry: Entry, image_path: Path, x: float, y: float, w: float, h: float) -> None:
    accent = HexColor(CATEGORY_COLORS[entry.category])
    pdf.setFillColor(HexColor("#F3F5F2"))
    pdf.roundRect(x, y, w, h, 5, fill=1, stroke=0)
    pdf.setStrokeColor(HexColor("#CAD2D1"))
    pdf.roundRect(x, y, w, h, 5, fill=0, stroke=1)
    pdf.setFillColor(accent)
    pdf.rect(x, y + h - 7, w, 7, fill=1, stroke=0)

    image_w = 142
    image_h = 84
    image_x = x + 10
    image_y = y + h - image_h - 17
    pdf.drawImage(ImageReader(str(image_path)), image_x, image_y, image_w, image_h, preserveAspectRatio=True, mask="auto")

    text_x = image_x + image_w + 12
    text_w = w - (text_x - x) - 12
    pdf.setFillColor(HexColor("#172127"))
    title_size = 12.4 if len(entry.name) < 22 else 10.8
    pdf.setFont("Helvetica-Bold", title_size)
    pdf.drawString(text_x, y + h - 25, entry.name)

    pdf.setFillColor(accent)
    pdf.setFont("Helvetica-Bold", 7.2)
    pdf.drawString(text_x, y + h - 39, entry.category.upper())

    pdf.setFillColor(HexColor("#526269"))
    pdf.setFont("Courier-Bold", 7.2)
    record = entry.record
    if stringWidth(record, "Courier-Bold", 7.2) > text_w:
        record = record[: max(8, int(text_w / 4.4) - 3)] + "..."
    pdf.drawString(text_x, y + h - 54, record)

    pdf.setFillColor(HexColor("#334148"))
    pdf.setFont("Helvetica", 8.0)
    desc_lines = wrap_lines(entry.description, "Helvetica", 8.0, text_w)
    cursor_y = y + h - 69
    for line_text in desc_lines[:5]:
        pdf.drawString(text_x, cursor_y, line_text)
        cursor_y -= 10.2


def make_pdf(image_paths: dict[str, Path]) -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    page_w, page_h = landscape(letter)
    pdf = canvas.Canvas(str(OUTPUT), pagesize=(page_w, page_h), pageCompression=1)
    pdf.setTitle("Power Pulley Panic - Complete Object Catalog")
    pdf.setAuthor("Power Pulley Panic Project")
    page_number = 1

    # Cover
    pdf.setFillColor(HexColor("#11191E"))
    pdf.rect(0, 0, page_w, page_h, fill=1, stroke=0)
    pdf.setFillColor(HexColor("#E58A25"))
    pdf.rect(0, page_h - 18, page_w, 18, fill=1, stroke=0)
    pdf.setFillColor(HexColor("#F0F3EF"))
    pdf.setFont("Helvetica-Bold", 36)
    pdf.drawString(46, page_h - 88, "POWER PULLEY PANIC")
    pdf.setFont("Helvetica-Bold", 24)
    pdf.setFillColor(HexColor("#58B7D3"))
    pdf.drawString(46, page_h - 123, "COMPLETE OBJECT CATALOG")
    pdf.setFillColor(HexColor("#AEBCC0"))
    pdf.setFont("Helvetica", 11)
    pdf.drawString(48, page_h - 150, f"83 visible gameplay objects - generated from the current game source")

    featured = ["Player", "Gear", "Trap Door", "Fan", "Water", "Enemy Robot"]
    for index, name in enumerate(featured):
        col = index % 3
        row = index // 3
        x = 47 + col * 244
        y = 250 - row * 145
        pdf.drawImage(ImageReader(str(image_paths[name])), x, y, 218, 126, preserveAspectRatio=True, mask="auto")
    pdf.setFillColor(HexColor("#72858C"))
    pdf.setFont("Helvetica", 8)
    pdf.drawString(48, 35, "Illustrations use shipped art where available and procedural diagrams matching the in-game industrial style.")
    draw_footer(pdf, page_number)
    pdf.showPage()
    page_number += 1

    # Scope and index page
    pdf.setFillColor(HexColor("#F4F6F3"))
    pdf.rect(0, 0, page_w, page_h, fill=1, stroke=0)
    pdf.setFillColor(HexColor("#172127"))
    pdf.setFont("Helvetica-Bold", 24)
    pdf.drawString(38, page_h - 48, "Catalog Scope")
    pdf.setFont("Helvetica", 10)
    scope = (
        "This catalog lists every visible gameplay object represented by the current Level, Machine, Fluid, "
        "GuideObject, Player, and runtime projectile/debris systems. True aliases are merged, while runtime-created "
        "objects remain separate entries. Nonvisual configuration records and implementation-only components are excluded."
    )
    y = page_h - 68
    for text_line in wrap_lines(scope, "Helvetica", 10, 710):
        pdf.drawString(40, y, text_line)
        y -= 13

    pdf.setFont("Helvetica-Bold", 15)
    pdf.drawString(40, y - 12, "Sections")
    y -= 38
    counts: dict[str, int] = {}
    for entry in ENTRIES:
        counts[entry.category] = counts.get(entry.category, 0) + 1
    for index, (category, count) in enumerate(counts.items()):
        col = index % 3
        row = index // 3
        x = 40 + col * 245
        box_y = y - row * 58
        pdf.setFillColor(HexColor(CATEGORY_COLORS[category]))
        pdf.roundRect(x, box_y - 32, 220, 40, 4, fill=1, stroke=0)
        pdf.setFillColor(HexColor("#102027"))
        pdf.setFont("Helvetica-Bold", 10)
        pdf.drawString(x + 10, box_y - 10, category)
        pdf.setFont("Helvetica", 9)
        pdf.drawRightString(x + 208, box_y - 10, f"{count} objects")

    pdf.setFillColor(HexColor("#334148"))
    pdf.setFont("Helvetica-Bold", 12)
    pdf.drawString(40, 110, "How to read each entry")
    pdf.setFont("Helvetica", 9)
    notes = [
        "The colored header identifies the gameplay category.",
        "The monospace line gives the level record or notes that the object is created at runtime.",
        "The illustration shows the object's recognizable in-game form or behavior.",
    ]
    for index, note in enumerate(notes):
        pdf.drawString(52, 91 - index * 15, f"{index + 1}. {note}")
    draw_footer(pdf, page_number)
    pdf.showPage()
    page_number += 1

    # Six entries per page, grouped by category without blank separator pages.
    card_w = 352
    card_h = 160
    x_positions = (32, 408)
    y_positions = (390, 214, 38)
    for start in range(0, len(ENTRIES), 6):
        pdf.setFillColor(HexColor("#E9EEEB"))
        pdf.rect(0, 0, page_w, page_h, fill=1, stroke=0)
        page_entries = ENTRIES[start:start + 6]
        section_names = []
        for entry in page_entries:
            if entry.category not in section_names:
                section_names.append(entry.category)
        pdf.setFillColor(HexColor("#172127"))
        pdf.setFont("Helvetica-Bold", 13)
        pdf.drawString(32, page_h - 24, " / ".join(section_names))
        for index, entry in enumerate(page_entries):
            col = index % 2
            row = index // 2
            draw_card(pdf, entry, image_paths[entry.name], x_positions[col], y_positions[row], card_w, card_h)
        draw_footer(pdf, page_number)
        pdf.showPage()
        page_number += 1

    pdf.save()


def main() -> None:
    TMP.mkdir(parents=True, exist_ok=True)
    image_paths: dict[str, Path] = {}
    for index, entry in enumerate(ENTRIES):
        safe_name = "".join(character.lower() if character.isalnum() else "_" for character in entry.name).strip("_")
        path = TMP / f"{index + 1:03d}_{safe_name}.png"
        draw_icon(entry, path)
        image_paths[entry.name] = path
    if len(ENTRIES) != 83:
        raise RuntimeError(f"Expected 83 catalog entries, found {len(ENTRIES)}")
    make_pdf(image_paths)
    print(OUTPUT)


if __name__ == "__main__":
    main()
