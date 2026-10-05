using System.ComponentModel;
using System.Globalization;

namespace PowerPulleyPanic.LevelEditor;

internal enum ObjectFieldKind { Number, Integer, Boolean, Text, Choice }

internal sealed record ObjectField(string Name, string Category = "Properties",
    ObjectFieldKind Kind = ObjectFieldKind.Number, string Description = "", string[]? Choices = null,
    bool Remainder = false, bool ReadOnly = false);

internal sealed class ObjectDetailsAdapter : ICustomTypeDescriptor
{
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
        ["solid"] = [X, Y, Width, Height], ["platform"] = [X, Y, Width, Height],
        ["ladder"] = [X, Y, Width, Height], ["cameraZone"] = [X, Y, Width, Height],
        ["darkness"] = [X, Y, Width, Height], ["exit"] = [X, Y,
            new("Width", "Size", ReadOnly: true), new("Height", "Size", ReadOnly: true)],
        ["button"] = [X, Y, Width, Height],
        ["playerStart"] = [X, Y], ["pulley"] = [X, Y],
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
        ["speedSensor"] = [X, Y, Width, Height, new("Minimum Speed", "Sensor"), Channel],
        ["beamSensor"] = [X, Y, new("Direction X", "Sensor"), new("Direction Y", "Sensor"), new("Length", "Sensor"), Channel],
        ["piston"] = [X, Y, Width, Height, new("Direction X", "Motion"), new("Direction Y", "Motion"),
            new("Travel", "Motion"), new("Speed", "Motion"), Channel],
        ["hydraulicCylinder"] = [X, Y, Width, Height, new("Direction X", "Motion"), new("Direction Y", "Motion"),
            new("Travel", "Motion"), new("Speed", "Motion"), Channel],
        ["rocketThruster"] = [X, Y, new("Direction X", "Motion"), new("Direction Y", "Motion"),
            new("Length", "Size"), Width, new("Strength", "Motion"), Channel],
        ["electricalArc"] = [X, Y, new("End X", "Transform"), new("End Y", "Transform"), new("Interval", "Hazard"), Channel],
        ["label"] = [X, Y, new("Text", "Content", ObjectFieldKind.Text, Remainder: true)],
        ["labelSized"] = [X, Y, new("Font Size", "Appearance", ObjectFieldKind.Integer),
            new("Text", "Content", ObjectFieldKind.Text, Remainder: true)],
        ["visualTile"] = [new("Layer", "Tile", ObjectFieldKind.Choice,
            Choices: ["background", "farBackground", "foreground"]), new("Column", "Tile", ObjectFieldKind.Integer),
            new("Row", "Tile", ObjectFieldKind.Integer), X, Y, new("Quarter Turns", "Tile", ObjectFieldKind.Integer),
            new("Flip Horizontal", "Tile", ObjectFieldKind.Boolean), new("Flip Vertical", "Tile", ObjectFieldKind.Boolean),
            new("Animation Frames", "Animation", ObjectFieldKind.Integer), new("Frame Seconds", "Animation")],
        ["clockFace"] = [X, Y, Radius],
        ["spikeHazard"] = [X, Y, Width, Height],
        ["spikePitTop"] = [new("Y", "Transform")],
        ["script"] = [new("Script", "Level", ObjectFieldKind.Text, Remainder: true)]
    };

    private readonly LevelObject item;
    private readonly ObjectField[] fields;
    private readonly List<string> values;

    internal string ObjectType => item.Type;
    internal int ObjectIndex => item.Index;
    internal string CurrentArguments => item.Arguments;
    internal string PreviousArguments { get; private set; } = string.Empty;

    internal ObjectDetailsAdapter(LevelObject item)
    {
        this.item = item;
        fields = GetSchema(item);
        values = ParseValues(item.Arguments, fields);
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
            new ReadOnlyObjectProperty("Index", "Object", item.Index)
        };
        properties.AddRange(fields.Select((field, index) => new ObjectFieldProperty(field, index)));
        return new PropertyDescriptorCollection([.. properties], true);
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
