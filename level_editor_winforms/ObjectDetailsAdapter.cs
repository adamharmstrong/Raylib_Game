using System.ComponentModel;
using System.Globalization;

namespace PowerPulleyPanic.LevelEditor;

internal enum ObjectFieldKind { Number, Integer, Boolean, Text, Choice }

internal sealed record ObjectField(string Name, string Category = "Properties",
    ObjectFieldKind Kind = ObjectFieldKind.Number, string Description = "", string[]? Choices = null,
    string[]? ChoiceValues = null, bool Remainder = false, bool ReadOnly = false);

internal sealed class ObjectDetailsAdapter : ICustomTypeDescriptor
{
    private static readonly string[] AnchorPoints =
        ["Top Left", "Top Center", "Top Right", "Middle Left", "Center", "Middle Right", "Bottom Left", "Bottom Center", "Bottom Right"];
    private static readonly ObjectField X = new("X", "Transform");
    private static readonly ObjectField Y = new("Y", "Transform");
    private static readonly ObjectField Width = new("Width", "Size", Description: "Width in world pixels.");
    private static readonly ObjectField Height = new("Height", "Size", Description: "Height in world pixels.");
    private static readonly ObjectField Radius = new("Radius", "Size");
    private static readonly ObjectField Mass = new("Mass", "Physics");
    private static readonly ObjectField Channel = new("Channel", "Power", ObjectFieldKind.Integer,
        "Objects with the same nonzero channel share power. Channel 0 operates independently.");

    private static readonly Dictionary<string, ObjectField[]> Schemas = new(StringComparer.OrdinalIgnoreCase)
    {
        ["bounds"] = [X, Y, Width, Height],
        ["author"] = [new("Author", "Level", ObjectFieldKind.Text, Remainder: true)],
        ["solid"] = [X, Y, Width, Height], ["platform"] = [X, Y, Width, Height],
        ["ladder"] = [X, Y, Width, Height], ["cameraZone"] = [X, Y, Width, Height],
        ["darkness"] = [X, Y, Width, Height], ["exit"] = [X, Y,
            new("Width", "Size", ReadOnly: true), new("Height", "Size", ReadOnly: true)],
        ["button"] = [X, Y, Width, Height],
        ["buttonPlatformLoop"] = [new("Button Index", "Connection", ObjectFieldKind.Integer,
            "-1 runs continuously; otherwise the zero-based activating button."), X, Y,
            new("Horizontal Radius", "Path"), new("Vertical Radius", "Path"),
            new("Platform Width", "Platforms"), new("Platform Height", "Platforms"),
            new("Speed", "Motion"), new("Platform Count", "Platforms", ObjectFieldKind.Integer)],
        ["platformLoopButton"] = [new("Loop Index", "Connection", ObjectFieldKind.Integer),
            new("Platform Index", "Connection", ObjectFieldKind.Integer), Width, Height,
            new("Button Index", "Connection", ObjectFieldKind.Integer)],
        ["buttonTrapDoor"] = [new("Button Index", "Connection", ObjectFieldKind.Integer),
            new("Trap Door Index", "Connection", ObjectFieldKind.Integer), new("Open Angle", "Motion"), new("Speed", "Motion")],
        ["buttonFan"] = [new("Button Index", "Connection", ObjectFieldKind.Integer),
            new("Fan Index", "Connection", ObjectFieldKind.Integer), new("Power", "Motion")],
        ["buttonExit"] = [new("Button Index", "Connection", ObjectFieldKind.Integer)],
        ["buttonLadder"] = [new("Button Index", "Connection", ObjectFieldKind.Integer), X, Y, Width, Height],
        ["buttonPlatform"] = [new("Button Index", "Connection", ObjectFieldKind.Integer), X, Y, Width, Height],
        ["buttonSpikeHazard"] = [new("Button Index", "Connection", ObjectFieldKind.Integer),
            new("Direction", "Direction", ObjectFieldKind.Choice, Choices: ["up", "down", "left", "right"]), X, Y, Width, Height],
        ["valveFluidFill"] = [new("Fluid Index", "Connection", ObjectFieldKind.Integer),
            new("Target Fill", "Fluid"), new("Rise Rate", "Fluid")],
        ["playerStart"] = [X, Y, new("Character", "Gameplay", ObjectFieldKind.Choice,
            "Default character for this level. The character-select screen can still override it.",
            Choices: ["Character 1", "Character 2", "Character 3", "Character 4"],
            ChoiceValues: ["0", "1", "2", "3"])], ["pulley"] = [X, Y],
        ["valve"] = [X, Y, Radius], ["pinwheel"] = [X, Y, Radius],
        ["water"] = FluidFields(), ["sand"] = FluidFields(), ["gel"] = FluidFields(), ["gas"] = FluidFields(),
        ["weight"] = [new("Pulley Index", Kind: ObjectFieldKind.Integer), Radius,
            new("Phase", "Motion"), new("Speed", "Motion"), Width, Height],
        ["rotaryLatch"] = [X, Y, Radius, new("Angle", "Rotation"), new("Target Angle", "Rotation"),
            new("Tolerance", "Rotation"), new("Spin Speed", "Rotation")],
        ["stoneBlock"] = [X, Y, Width, Height, Mass],
        ["boulder"] = [X, Y, Radius, Mass], ["physicsWheel"] = [X, Y, Radius, Mass],
        ["gear"] = [new("Visual Type", "Appearance", ObjectFieldKind.Choice,
            Choices: ["standard", "clock", "ratchet"]), new("Mounting", "Physics", ObjectFieldKind.Choice,
            Choices: ["dynamic", "mounted"]), new("Orientation", "Transform", ObjectFieldKind.Choice,
            Choices: ["vertical", "horizontal"]), X, Y, Radius, Mass,
            new("Teeth", "Appearance", ObjectFieldKind.Integer), new("Rotation", "Rotation"),
            new("Angular Velocity", "Motion"), new("Drive Speed", "Motion"),
            new("Clock Hand", "Appearance", ObjectFieldKind.Choice, Choices: ["none", "hour", "minute"])],
        ["flywheel"] = [X, Y, Radius, Mass, new("Angular Velocity", "Motion")],
        ["steeringWheel"] = [X, Y, Radius, new("Rotation", "Rotation")],
        ["screw"] = [X, Y, new("Length", "Size"), Radius, new("Angle", "Rotation"), new("Spin Speed", "Motion")],
        ["fan"] = [X, Y, new("Direction X", "Direction"), new("Direction Y", "Direction"),
            new("Length", "Size"), Width, new("Strength", "Motion"), new("Power", "Motion")],
        ["seeSaw"] = [X, Y, new("Length", "Size"), new("Thickness", "Size"),
            new("Minimum Angle", "Rotation"), new("Maximum Angle", "Rotation"), new("Response", "Physics")],
        ["wedge"] = [X, Y, new("Length", "Size"), new("Thickness", "Size"), new("Angle", "Rotation"),
            new("Segments", "Appearance", ObjectFieldKind.Integer)],
        ["ramp"] = [X, Y, new("Length", "Size"), new("Thickness", "Size"), new("Angle", "Rotation"),
            new("Segments", "Appearance", ObjectFieldKind.Integer)],
        ["trapDoor"] = [X, Y, new("Length", "Size"), new("Thickness", "Size"), new("Angle", "Rotation"),
            new("Style", "Appearance", ObjectFieldKind.Choice, Choices: ["standard", "minimal"])],
        ["chain"] = LineFields([new("Spacing", "Appearance"), new("Scale", "Appearance"),
            new("Pin Start", "Physics", ObjectFieldKind.Boolean), new("Pin End", "Physics", ObjectFieldKind.Boolean)]),
        ["physicsRope"] = LineFields([new("Length", "Size"), new("Thickness", "Size"),
            new("Pin Start", "Physics", ObjectFieldKind.Boolean), new("Pin End", "Physics", ObjectFieldKind.Boolean)]),
        ["portalPair"] = [new("Entrance X", "Entrance"), new("Entrance Y", "Entrance"),
            new("Entrance Width", "Entrance"), new("Entrance Height", "Entrance"), new("Exit X", "Exit"),
            new("Exit Y", "Exit"), new("Exit Width", "Exit"), new("Exit Height", "Exit")],
        ["directionalSpikeHazard"] = [new("Direction", "Direction", ObjectFieldKind.Choice,
            Choices: ["up", "down", "left", "right"]), X, Y, Width, Height],
        ["arrowTrap"] = [X, Y, new("Direction X", "Direction"), new("Direction Y", "Direction"),
            new("Interval", "Timing"), new("Speed", "Motion")],
        ["breakableTile"] = [X, Y, Width, Height, new("Break Delay", "Timing")],
        ["enemy"] = [X, Y, Width, Height, new("Patrol Minimum X", "Motion"),
            new("Patrol Maximum X", "Motion"), new("Speed", "Motion")],
        ["checkpoint"] = [X, Y, Width, Height],
        ["collectible"] = [X, Y, Radius],
        ["ball"] = [X, Y, Radius, Mass],
        ["barrel"] = [X, Y, Width, Height, Mass],
        ["movingPlatform"] = MovingPlatformFields(), ["elevator"] = MovingPlatformFields(),
        ["pendulumBob"] = [X, Y, new("Length", "Size"), Radius, new("Phase", "Motion"), new("Speed", "Motion")],
        ["oneWayPlatform"] = [X, Y, Width, Height], ["guideRail"] = [X, Y, Width, Height],
        ["ceilingHook"] = [X, Y, Radius], ["fixedJoint"] = [X, Y, Radius],
        ["rod"] = ConstraintFields(false),
        ["spring"] = ConstraintFields(true), ["compressionSpring"] = ConstraintFields(true),
        ["extensionSpring"] = ConstraintFields(true), ["torsionSpring"] = ConstraintFields(true),
        ["garterSpring"] = ConstraintFields(true), ["voluteSpring"] = ConstraintFields(true),
        ["spiralSpring"] = ConstraintFields(true), ["constantForceSpring"] = ConstraintFields(true),
        ["constantTorqueSpring"] = ConstraintFields(true), ["leafSpring"] = ConstraintFields(true),
        ["beamSpring"] = ConstraintFields(true), ["discSpring"] = ConstraintFields(true),
        ["waveSpring"] = ConstraintFields(true), ["waveWasher"] = ConstraintFields(true),
        ["torsionBar"] = ConstraintFields(true), ["ringSpring"] = ConstraintFields(true),
        ["elastomerSpring"] = ConstraintFields(true), ["pneumaticSpring"] = ConstraintFields(true),
        ["gasSpring"] = ConstraintFields(true), ["hydropneumaticSpring"] = ConstraintFields(true),
        ["magneticSpring"] = ConstraintFields(true), ["compositeSpring"] = ConstraintFields(true),
        ["crank"] = RotaryMachineFields(false), ["ratchet"] = RotaryMachineFields(false),
        ["clutch"] = RotaryMachineFields(true),
        ["brake"] = [X, Y, Width, Height, new("Strength", "Machine"),
            new("Channel", "Power", ObjectFieldKind.Integer)],
        ["battery"] = [X, Y, Width, Height, new("Output", "Power"), Channel],
        ["generator"] = [X, Y, Width, Height, new("Rated Shaft Speed", "Power"),
            new("Maximum Output", "Power"), new("Efficiency", "Power"), Channel],
        ["electricMotor"] = [X, Y, Radius, new("Speed", "Motion"), Channel],
        ["turntable"] = [X, Y, Radius, new("Speed", "Motion"), Channel],
        ["cam"] = [X, Y, Radius, new("Speed", "Motion"), new("Eccentricity", "Motion"), Channel],
        ["relay"] = [X, Y, Width, Height, Channel],
        ["limitSwitch"] = [X, Y, Width, Height, Channel],
        ["fuse"] = [X, Y, Width, Height, new("Capacity", "Power"), Channel],
        ["magnet"] = [X, Y, new("Range", "Size"), new("Strength", "Physics"), Channel],
        ["antiGravityPad"] = [X, Y, Width, Height, new("Field Height", "Field"), new("Strength", "Motion"), Channel],
        ["tripodDerrick"] = [X, Y, Width, Height],
        ["chainHoist"] = [X, Y, new("Travel", "Motion"), new("Speed", "Motion"), new("Strength", "Motion"), Channel],
        ["blockAndTackle"] = [X, Y, new("Travel", "Motion"), new("Speed", "Motion"), new("Strength", "Motion"), Channel],
        ["comeAlong"] = [X, Y, new("Anchor X", "Anchor"), new("Anchor Y", "Anchor"), new("Speed", "Motion"), new("Strength", "Motion"), Channel],
        ["manualBenchGrinder"] = [X, Y, Radius, new("Speed", "Motion")],
        ["loadCart"] = [X, Y, Width, Height, Mass],
        ["revolvingStoneGate"] = [X, Y, Width, Height, new("Balance", "Physics")],
        ["rockHammer"] = [X, Y, Width, Height, Mass],
        ["fieldCoil"] = [X, Y, Radius, new("Strength", "Motion"), Channel],
        ["spokePress"] = [X, Y, Width, Height, new("Direction X", "Motion"), new("Direction Y", "Motion"),
            new("Travel", "Motion"), new("Speed", "Motion"), Channel],
        ["polarisTelescope"] = [X, Y, new("Length", "Size"), new("Angle", "Rotation"), Channel],
        ["leedskalninSundial"] = [X, Y, Radius, new("Interval", "Timing"), Channel],
        ["perpetualMotionHolder"] = [X, Y, Width, Height, new("Hold Time", "Timing"), new("Strength", "Motion"), Channel],
        ["bellows"] = [X, Y, Width, Height, new("Direction X", "Motion"), new("Direction Y", "Motion"),
            new("Reach", "Motion"), new("Strength", "Motion")],
        ["trampoline"] = [X, Y, Width, Height, new("Bounce Strength", "Physics")],
        ["boxingGlove"] = [X, Y, Width, Height, new("Direction X", "Motion"), new("Direction Y", "Motion"),
            new("Reach", "Motion"), new("Strength", "Motion"), Channel],
        ["vacuum"] = [X, Y, Width, Height, new("Direction X", "Motion"), new("Direction Y", "Motion"),
            new("Range", "Motion"), new("Strength", "Motion"), Channel],
        ["magnifyingGlass"] = [X, Y, Radius, new("Direction X", "Direction"), new("Direction Y", "Direction"),
            new("Focus Distance", "Optics")],
        ["lightSource"] = [X, Y, new("Direction X", "Direction"), new("Direction Y", "Direction"),
            new("Beam Length", "Optics"), new("Beam Width", "Optics"), Channel],
        ["ignitableFuse"] = [X, Y, Width, Height, Channel],
        ["programmableBall"] = [X, Y, Radius, Mass, new("Bounciness", "Physics"),
            new("Friction", "Physics"), new("Gravity Scale", "Physics")],
        ["transRotoConverter"] = [X, Y, Radius, new("Ratio", "Machine")],
        ["rotoTransConverter"] = [X, Y, Width, Height, new("Direction X", "Motion"),
            new("Direction Y", "Motion"), new("Travel", "Motion"), new("Speed", "Motion"), Channel],
        ["acceleratorTube"] = [X, Y, Width, Height, new("Direction X", "Motion"),
            new("Direction Y", "Motion"), new("Acceleration", "Motion"), Channel],
        ["pinballBumper"] = [X, Y, Radius, new("Kick Strength", "Physics")],
        ["pinballFlipper"] = [X, Y, Width, Height, new("Direction", "Motion"),
            new("Kick Strength", "Physics"), Channel],
        ["laserEmitter"] = [X, Y, new("Direction X", "Direction"), new("Direction Y", "Direction"),
            new("Beam Length", "Optics"), new("Color", "Optics", ObjectFieldKind.Choice,
                Choices: ["red", "green", "blue", "yellow", "magenta", "cyan", "white"]), Channel],
        ["laserMixer"] = [X, Y, Width, Height, new("Direction X", "Direction"),
            new("Direction Y", "Direction"), new("Beam Length", "Optics")],
        ["laserDetector"] = [X, Y, Width, Height, new("Color", "Optics", ObjectFieldKind.Choice,
            Choices: ["red", "green", "blue", "yellow", "magenta", "cyan", "white"]), Channel],
        ["hotAirBalloon"] = [X, Y, Radius, Mass, new("Lift", "Physics")],
        ["blimp"] = [X, Y, Width, Height, Mass, new("Lift", "Physics")],
        ["leakyBucket"] = [X, Y, Width, Height, Mass, new("Fluid Amount", "Fluid"), new("Leak Rate", "Fluid")],
        ["destructibleWood"] = [X, Y, Width, Height, new("Durability", "Physics")],
        ["destructibleBrick"] = [X, Y, Width, Height, new("Durability", "Physics")],
        ["destructibleGlass"] = [X, Y, Width, Height, new("Durability", "Physics")],
        ["glassPane"] = [X, Y, Width, Height, new("Durability", "Physics")],
        ["glassBlock"] = [X, Y, Width, Height, new("Durability", "Physics")],
        ["iceBlock"] = [X, Y, Width, Height, new("Melt Resistance", "Thermal")],
        ["iceSheet"] = [X, Y, Width, Height, new("Melt Resistance", "Thermal")],
        ["heater"] = [X, Y, Width, Height, new("Direction X", "Thermal"), new("Direction Y", "Thermal"),
            new("Heat Range", "Thermal"), new("Beam Width", "Thermal"), new("Heat Output", "Thermal"), Channel],
        ["campfire"] = [X, Y, Radius, new("Heat Radius", "Thermal"), new("Heat Output", "Thermal")],
        ["torch"] = [X, Y, Width, Height, Mass, new("Heat Radius", "Thermal"), new("Heat Output", "Thermal")],
        ["waterBucket"] = [X, Y, Width, Height, Mass, new("Water Amount", "Fluid"),
            new("Pour Rate", "Fluid"), new("Rotation", "Transform")],
        ["signalDust"] = [X, Y, new("End X", "Signal"), new("End Y", "Signal"), Channel],
        ["ternaryTrace"] = [X, Y, new("End X", "Signal"), new("End Y", "Signal"),
            new("Negative Output Channel", "Signal", ObjectFieldKind.Integer),
            new("Positive Output Channel", "Signal", ObjectFieldKind.Integer)],
        ["ternarySwitch"] = [X, Y, Width, Height, new("Initial State", "Signal", ObjectFieldKind.Integer)],
        ["speedSensor"] = [X, Y, Width, Height, new("Minimum Speed", "Sensor"), Channel],
        ["beamSensor"] = [X, Y, new("Direction X", "Sensor"), new("Direction Y", "Sensor"), new("Length", "Sensor"), Channel],
        ["piston"] = [X, Y, Width, Height, new("Direction X", "Motion"), new("Direction Y", "Motion"),
            new("Travel", "Motion"), new("Speed", "Motion"), Channel],
        ["hydraulicCylinder"] = [X, Y, Width, Height, new("Direction X", "Motion"), new("Direction Y", "Motion"),
            new("Travel", "Motion"), new("Speed", "Motion"), Channel],
        ["rocketThruster"] = [X, Y, new("Direction X", "Motion"), new("Direction Y", "Motion"),
            new("Length", "Size"), Width, new("Strength", "Motion"), Channel],
        ["electricalArc"] = [X, Y, new("End X", "Transform"), new("End Y", "Transform"), new("Interval", "Hazard"), Channel],
        ["teleforce"] = [X, Y, Width, Height, new("Direction X", "Direction"), new("Direction Y", "Direction"),
            new("Range", "Hazard"), new("Beam Width", "Hazard"), new("Force", "Hazard"),
            new("Charge Seconds", "Timing"), new("Pulse Seconds", "Timing"), new("Cooldown Seconds", "Timing"), Channel],
        ["rock"] = [X, Y, Width, Height, new("Rotation", "Transform")],
        ["teslaCoil"] = [X, Y, Width, Height, new("Arc Range", "Hazard"), new("Arc Force", "Hazard"),
            new("Charge Seconds", "Timing"), new("Pulse Seconds", "Timing"), new("Cooldown Seconds", "Timing"),
            new("Input Channel", "Power", ObjectFieldKind.Integer), new("Wireless Output Channel", "Power", ObjectFieldKind.Integer)],
        ["thornyVines"] = [X, Y, Width, Height, new("Damage Per Second", "Hazard")],
        ["label"] = [X, Y, new("Text", "Content", ObjectFieldKind.Text, Remainder: true)],
        ["labelSized"] = [X, Y, new("Font Size", "Appearance", ObjectFieldKind.Integer),
            new("Text", "Content", ObjectFieldKind.Text, Remainder: true)],
        ["visualTile"] = [new("Layer", "Tile", ObjectFieldKind.Choice,
            Choices: ["background", "farBackground", "middleground", "foreground"]), new("Column", "Tile", ObjectFieldKind.Integer),
            new("Row", "Tile", ObjectFieldKind.Integer), X, Y, new("Quarter Turns", "Tile", ObjectFieldKind.Integer),
            new("Flip Horizontal", "Tile", ObjectFieldKind.Boolean), new("Flip Vertical", "Tile", ObjectFieldKind.Boolean),
            new("Animation Frames", "Animation", ObjectFieldKind.Integer), new("Frame Seconds", "Animation")],
        ["clockFace"] = [X, Y, Radius],
        ["waterPit"] = [X, Y, Width, Height, new("Surface Y", "Water"),
            new("Target Surface Y", "Water"), new("Fill Rate", "Water")],
        ["toxinLeak"] = [new("Fluid Index", "Gas", ObjectFieldKind.Integer), X, Y,
            new("Mass Per Second", "Gas"), new("Maximum Mass", "Gas"), new("Exposure Rate", "Gas")],
        ["conveyorBelt"] = [X, Y, Width, Height, new("Speed", "Motion")],
        ["crushingBlock"] = MovingPlatformFields(),
        ["swingingHammer"] = [X, Y, new("Length", "Size"), Radius, new("Phase", "Motion"), new("Speed", "Motion")],
        ["sawBlade"] = [X, Y, Radius, new("Direction X", "Motion"), new("Direction Y", "Motion"),
            new("Travel", "Motion"), new("Speed", "Motion")],
        ["spinnerTrap"] = [X, Y, Radius, new("Speed", "Motion")],
        ["steamVent"] = [X, Y, new("Direction X", "Motion"), new("Direction Y", "Motion"),
            new("Length", "Size"), Width, new("Strength", "Motion"), new("Interval", "Timing")],
        ["oil"] = [X, Y, Width, Height], ["mud"] = [X, Y, Width, Height],
        ["key"] = [X, Y, Radius], ["gasMask"] = [X, Y, Radius], ["scubaGear"] = [X, Y, Radius],
        ["breakableCrate"] = [X, Y, Width, Height, Mass],
        ["explosiveBarrel"] = [X, Y, Width, Height, Mass],
        ["tnt"] = [X, Y, Width, Height, Mass, new("Fuse Seconds", "Hazard"), new("Blast Radius", "Hazard")],
        ["spikeHazard"] = [X, Y, Width, Height],
        ["spikePitTop"] = [new("Y", "Transform")],
        ["script"] = [new("Script", "Level", ObjectFieldKind.Text, Remainder: true)]
    };

    private readonly LevelObject item;
    private readonly ObjectField[] fields;
    private readonly List<string> values;
    internal int AnchorPointIndex { get; private set; }

    internal string ObjectType => item.Type;
    internal int ObjectIndex => item.Index;
    internal string CurrentArguments => item.Arguments;
    internal string PreviousArguments { get; private set; } = string.Empty;
    internal LevelObject Item => item;

    internal void SetAnchorPointIndex(int index) => AnchorPointIndex = Math.Clamp(index, 0, AnchorPoints.Length - 1);

    internal ObjectDetailsAdapter(LevelObject item, int anchorPointIndex = 4)
    {
        this.item = item;
        fields = GetSchema(item);
        values = ParseValues(item.Arguments, fields);
        AnchorPointIndex = Math.Clamp(anchorPointIndex, 0, AnchorPoints.Length - 1);
    }

    private static ObjectField[] GetSchema(LevelObject item)
    {
        if (item.Type.Equals("gear", StringComparison.OrdinalIgnoreCase))
        {
            var first = item.Arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries).FirstOrDefault();
            if (double.TryParse(first, NumberStyles.Float, CultureInfo.InvariantCulture, out _))
                return [X, Y, Radius, Mass, new("Orientation", "Transform", ObjectFieldKind.Choice,
                    Choices: ["vertical", "horizontal"])];
        }
        return Schemas.TryGetValue(item.Type, out var schema)
            ? schema : [new("Arguments", "Level Data", ObjectFieldKind.Text, Remainder: true)];
    }

    private static ObjectField[] FluidFields() => [X, Y, Width, Height,
        new("Particle Spacing", "Simulation"), new("Initial Fill", "Simulation"), new("Flow Speed", "Simulation")];

    private static ObjectField[] LineFields(ObjectField[] tail) =>
        [new("Start X", "Start"), new("Start Y", "Start"), new("End X", "End"), new("End Y", "End"), .. tail];

    private static ObjectField[] MovingPlatformFields() => [X, Y, Width, Height,
        new("Direction X", "Motion"), new("Direction Y", "Motion"), new("Travel Distance", "Motion"),
        new("Speed", "Motion")];

    private static ObjectField[] ConstraintFields(bool spring) => spring
        ? [new("Start X", "Start"), new("Start Y", "Start"), new("End X", "End"),
            new("End Y", "End"), new("Width", "Appearance"), new("Stiffness", "Physics"),
            new("Damping", "Physics")]
        : [new("Start X", "Start"), new("Start Y", "Start"), new("End X", "End"),
            new("End Y", "End"), new("Width", "Appearance")];

    private static ObjectField[] RotaryMachineFields(bool clutch) => clutch
        ? [X, Y, Radius, new("Speed", "Motion"), new("Engaged", "Machine", ObjectFieldKind.Boolean),
            new("Channel", "Power", ObjectFieldKind.Integer)]
        : [X, Y, Radius, new("Speed", "Motion"), new("Channel", "Power", ObjectFieldKind.Integer)];

    private static List<string> ParseValues(string arguments, ObjectField[] schema)
    {
        var tokens = arguments.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        var result = new List<string>(schema.Length);
        var token = 0;
        foreach (var field in schema)
        {
            if (field.Remainder)
            {
                result.Add(token < tokens.Length ? string.Join(' ', tokens[token..]) : string.Empty);
                token = tokens.Length;
            }
            else result.Add(token < tokens.Length ? tokens[token++] : DefaultValue(field.Kind));
        }
        return result;
    }

    private static string DefaultValue(ObjectFieldKind kind) => kind == ObjectFieldKind.Boolean ? "0" : "0";

    internal object GetValue(int index)
    {
        var raw = values[index];
        return fields[index].Kind switch
        {
            ObjectFieldKind.Integer => int.TryParse(raw, NumberStyles.Integer, CultureInfo.InvariantCulture, out var integer) ? integer : 0,
            ObjectFieldKind.Boolean => raw == "1" || bool.TryParse(raw, out var boolean) && boolean,
            ObjectFieldKind.Number => double.TryParse(raw, NumberStyles.Float, CultureInfo.InvariantCulture, out var number) ? number : 0d,
            ObjectFieldKind.Choice when fields[index].ChoiceValues is { } choiceValues && fields[index].Choices is { } choices =>
                choices[Math.Clamp(Array.IndexOf(choiceValues, raw), 0, choices.Length - 1)],
            _ => raw
        };
    }

    internal void SetValue(int index, object? value)
    {
        PreviousArguments = item.Arguments;
        values[index] = fields[index].Kind switch
        {
            ObjectFieldKind.Boolean => value is true ? "1" : "0",
            ObjectFieldKind.Number => Convert.ToDouble(value, CultureInfo.InvariantCulture).ToString("0.###", CultureInfo.InvariantCulture),
            ObjectFieldKind.Integer => Convert.ToInt32(value, CultureInfo.InvariantCulture).ToString(CultureInfo.InvariantCulture),
            ObjectFieldKind.Choice when fields[index].ChoiceValues is { } choiceValues && fields[index].Choices is { } choices =>
                choiceValues[Math.Max(0, Array.IndexOf(choices, value?.ToString()))],
            _ => value?.ToString()?.Trim() ?? string.Empty
        };
        item.Arguments = string.Join(' ', values).Trim();
    }

    public AttributeCollection GetAttributes() => AttributeCollection.Empty;
    public string? GetClassName() => nameof(ObjectDetailsAdapter);
    public string? GetComponentName() => item.DisplayName;
    public TypeConverter GetConverter() => new();
    public EventDescriptor? GetDefaultEvent() => null;
    public PropertyDescriptor? GetDefaultProperty() => null;
    public object? GetEditor(Type editorBaseType) => null;
    public EventDescriptorCollection GetEvents(Attribute[]? attributes) => EventDescriptorCollection.Empty;
    public EventDescriptorCollection GetEvents() => EventDescriptorCollection.Empty;
    public object GetPropertyOwner(PropertyDescriptor? pd) => this;
    public PropertyDescriptorCollection GetProperties() => GetProperties(null);
    public PropertyDescriptorCollection GetProperties(Attribute[]? attributes)
    {
        var properties = new List<PropertyDescriptor>
        {
            new ReadOnlyObjectProperty("Type", "Object", item.Type),
            new ReadOnlyObjectProperty("Index", "Object", item.Index),
            new AnchorPointProperty(AnchorPoints)
        };
        properties.AddRange(fields.Select((field, index) => new ObjectFieldProperty(field, index)));
        properties.Add(new ScriptProperty());
        properties.Add(new SoundProperty());
        return new PropertyDescriptorCollection([.. properties], true);
    }

    private sealed class SoundProperty() : PropertyDescriptor("Sound", null)
    {
        public override Type ComponentType => typeof(ObjectDetailsAdapter);
        public override Type PropertyType => typeof(string);
        public override bool IsReadOnly => false;
        public override string Category => "Audio";
        public override string Description => "Plays when the player enters this object's bounds. Use Objects > Attach Sound Effect to import a file. Clear to remove.";
        public override object GetValue(object? component) => ((ObjectDetailsAdapter)component!).item.Sound;
        public override void SetValue(object? component, object? value) {
            ((ObjectDetailsAdapter)component!).item.Sound = value as string ?? "";
            OnValueChanged(component, EventArgs.Empty);
        }
        public override bool CanResetValue(object component) => true;
        public override void ResetValue(object component) => SetValue(component, "");
        public override bool ShouldSerializeValue(object component) => false;
    }

    private sealed class ScriptProperty() : PropertyDescriptor("Scripts", null)
    {
        public override Type ComponentType => typeof(ObjectDetailsAdapter);
        public override Type PropertyType => typeof(string[]);
        public override bool IsReadOnly => false;
        public override string Category => "Scripts";
        public override string Description => "Lua and C++ paths attached to this instance. Use Objects > Attach Lua Script or Attach C++ Script to browse. Build Scripts compiles C++; Play builds automatically.";
        public override object GetValue(object? component) => ((ObjectDetailsAdapter)component!).item.Scripts;
        public override void SetValue(object? component, object? value)
        {
            ((ObjectDetailsAdapter)component!).item.Scripts = value as string[] ?? [];
            OnValueChanged(component, EventArgs.Empty);
        }
        public override bool CanResetValue(object component) => true;
        public override void ResetValue(object component) => SetValue(component, Array.Empty<string>());
        public override bool ShouldSerializeValue(object component) => false;
    }

    private void SetAnchorPoint(string value)
    {
        var index = Array.IndexOf(AnchorPoints, value);
        if (index >= 0) AnchorPointIndex = index;
    }

    private sealed class AnchorPointProperty(string[] choices) : PropertyDescriptor("Anchor Point", null)
    {
        public override Type ComponentType => typeof(ObjectDetailsAdapter);
        public override bool IsReadOnly => false;
        public override Type PropertyType => typeof(string);
        public override string Category => "Transform";
        public override string Description => "Pivot used by the transform gizmo.";
        public override TypeConverter Converter => new ChoiceConverter(choices);
        public override bool CanResetValue(object component) => false;
        public override object GetValue(object? component)
        {
            var adapter = (ObjectDetailsAdapter)component!;
            return choices[adapter.AnchorPointIndex];
        }
        public override void ResetValue(object component) { }
        public override void SetValue(object? component, object? value)
        {
            ((ObjectDetailsAdapter)component!).SetAnchorPoint(value?.ToString() ?? string.Empty);
            OnValueChanged(component, EventArgs.Empty);
        }
        public override bool ShouldSerializeValue(object component) => false;
    }

    private sealed class ObjectFieldProperty(ObjectField definition, int index) : PropertyDescriptor(definition.Name, null)
    {
        public override Type ComponentType => typeof(ObjectDetailsAdapter);
        public override bool IsReadOnly => definition.ReadOnly;
        public override Type PropertyType => definition.Kind switch
        {
            ObjectFieldKind.Number => typeof(double), ObjectFieldKind.Integer => typeof(int),
            ObjectFieldKind.Boolean => typeof(bool), _ => typeof(string)
        };
        public override string Category => definition.Category;
        public override string Description => definition.Description;
        public override TypeConverter Converter => definition.Kind == ObjectFieldKind.Choice
            ? new ChoiceConverter(definition.Choices ?? []) : base.Converter;
        public override bool CanResetValue(object component) => false;
        public override object? GetValue(object? component) => ((ObjectDetailsAdapter)component!).GetValue(index);
        public override void ResetValue(object component) { }
        public override void SetValue(object? component, object? value)
        {
            ((ObjectDetailsAdapter)component!).SetValue(index, value);
            OnValueChanged(component, EventArgs.Empty);
        }
        public override bool ShouldSerializeValue(object component) => false;
    }

    private sealed class ReadOnlyObjectProperty(string name, string category, object value) : PropertyDescriptor(name, null)
    {
        public override Type ComponentType => typeof(ObjectDetailsAdapter);
        public override bool IsReadOnly => true;
        public override Type PropertyType => value.GetType();
        public override string Category => category;
        public override bool CanResetValue(object component) => false;
        public override object GetValue(object? component) => value;
        public override void ResetValue(object component) { }
        public override void SetValue(object? component, object? newValue) { }
        public override bool ShouldSerializeValue(object component) => false;
    }

    private sealed class ChoiceConverter(string[] choices) : StringConverter
    {
        public override bool GetStandardValuesSupported(ITypeDescriptorContext? context) => true;
        public override bool GetStandardValuesExclusive(ITypeDescriptorContext? context) => true;
        public override StandardValuesCollection GetStandardValues(ITypeDescriptorContext? context) => new(choices);
    }
}
