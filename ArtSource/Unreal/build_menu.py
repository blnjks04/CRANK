"""Build /Game/Crank/Maps/Lvl_MainMenu: the clockmaker's workbench at night with two dolls winding each other."""
import unreal

eal = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MAP = "/Game/Crank/Maps/Lvl_MainMenu"
ENV = "/Game/Crank/Environment/"
PROPS = "/Game/Crank/Props/"
DOLL = "/Game/Crank/Characters/Doll/"
MATS = "/Game/Crank/Materials/"
CAM_LOC, CAM_YAW, CAM_PITCH = (2840, 2530, 712), -90.0, 6.0


def V(x, y, z):
    return unreal.Vector(x, y, z)


def R(yaw=0.0, pitch=0.0, roll=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def mesh_actor(mesh, loc=(0, 0, 0), yaw=0.0, name=None, folder="Environment", scale=None):
    a = actors.spawn_actor_from_object(eal.load_asset(mesh), V(*loc), R(yaw))
    a.set_actor_label(name or mesh.rsplit("/", 1)[-1])
    a.set_folder_path(folder)
    if scale:
        a.set_actor_scale3d(V(*scale))
    return a


def native_actor(cls, loc, yaw=0.0, name=None, folder="Lighting", pitch=0.0):
    a = actors.spawn_actor_from_class(cls, V(*loc), R(yaw, pitch))
    a.set_actor_label(name or cls.__name__)
    a.set_folder_path(folder)
    return a


def paint_instance(name, color):
    path = MATS + name
    if eal.does_asset_exist(path):
        mi = eal.load_asset(path)
    else:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        mi = tools.create_asset(name, MATS[:-1], unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", eal.load_asset(MATS + "M_DollPaint"))
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "PaintColor", unreal.LinearColor(*color, 1.0))
    unreal.MaterialEditingLibrary.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


def doll(name, loc, yaw, anim, paint, key_tags=("MenuKey",)):
    """Skeletal mesh doll playing a looping animation, with its wind-up key riding the spine bone."""
    a = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, V(*loc), R(yaw))
    a.set_actor_label(name)
    a.set_folder_path("Dolls")
    comp = a.get_component_by_class(unreal.SkeletalMeshComponent)
    comp.set_skinned_asset_and_update(eal.load_asset(DOLL + "SK_Doll"))
    comp.set_material(0, paint)
    comp.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    data = unreal.SingleAnimationPlayData()
    data.set_editor_property("anim_to_play", eal.load_asset(DOLL + "Anims/" + anim))
    data.set_editor_property("saved_looping", True)
    data.set_editor_property("saved_playing", True)
    data.set_editor_property("saved_play_rate", 1.0)
    comp.set_editor_property("animation_data", data)

    # key: 18 cm behind and 58 cm above the feet, its X axis pointing out of the back
    t = a.get_actor_transform()
    key_loc = unreal.MathLibrary.transform_location(t, V(-18.0, 0.0, 58.0))
    key = actors.spawn_actor_from_object(eal.load_asset(DOLL + "SM_Doll_Key"), key_loc, R(yaw + 180.0))
    key.set_actor_label(name + "_Key")
    key.set_folder_path("Dolls")
    key.tags = list(key_tags)
    key.get_component_by_class(unreal.StaticMeshComponent).set_mobility(unreal.ComponentMobility.MOVABLE)
    key.attach_to_component(comp, "spine", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                            unreal.AttachmentRule.KEEP_WORLD, False)
    return a


# ------------------------------------------------------------------ new level
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
if eal.does_asset_exist(MAP):
    eal.delete_asset(MAP)

for m in ["SM_Floor", "SM_Wall_North", "SM_Wall_South", "SM_Wall_East", "SM_Wall_West", "SM_Ceiling", "SM_Bookcase",
          "SM_CrateStack", "SM_Workbench", "SM_RulerRamp", "SM_EntryWedge", "SM_SinkCabinet", "SM_CabinetClutter",
          "SM_Counter", "SM_Fridge", "SM_TableTop"]:
    mesh_actor(ENV + m)
for i, (x, y) in enumerate([(1950, 650), (4150, 650), (1950, 2850), (4150, 2850)]):
    mesh_actor(ENV + "SM_TableLeg", (x, y, 0), name="TableLeg_%d" % i)
mesh_actor(PROPS + "SM_Rug", (800, 6050, 0), folder="Decor")
mesh_actor(PROPS + "SM_Nightlight", (300, 7600, 0), folder="Decor")
mesh_actor(PROPS + "SM_BookStack", (5150, 5940, 0), folder="Decor")
mesh_actor(PROPS + "SM_Pot_A", (6360, 5850, 0), folder="Decor")
mesh_actor(PROPS + "SM_Pot_B", (7450, 5650, 0), folder="Decor")
mesh_actor(PROPS + "SM_Pot_C", (6000, 5150, 0), folder="Decor")
mesh_actor(PROPS + "SM_ClockMovement", (480, 4430, 300), folder="Workbench")
mesh_actor(PROPS + "SM_Part_MainSpring", (470, 4420, 445), yaw=30, folder="Workbench")
mesh_actor(PROPS + "SM_GrandfatherClock", (250, 1600, 0), folder="Decor")

# ------------------------------------------------------------------ the two dolls (blue is being wound by red)
red = paint_instance("MI_DollPaint_Red", (0.80, 0.12, 0.08))
blue = paint_instance("MI_DollPaint_Blue", (0.10, 0.32, 0.85))
TABLE_Z = 656.0
doll("Doll_Blue", (2890, 2160, TABLE_Z), 180.0, "A_Doll_Wound", blue, ("MenuKey", "MenuKeyFast"))
doll("Doll_Red", (2948, 2160, TABLE_Z), 180.0, "A_Doll_Wind", red)
for i, (x, y, yaw) in enumerate([(2600, 3060, -90), (3550, 3060, -90), (2600, 440, 90), (3550, 440, 90)]):
    mesh_actor(ENV + "SM_Chair", (x, y, 0), yaw=yaw, name="Chair_%d" % i)

# ------------------------------------------------------------------ lighting (night kitchen, warm desk light)
moon = native_actor(unreal.DirectionalLight, (3000, -500, 2500), 70, "MoonLight", pitch=-32)
mc = moon.get_component_by_class(unreal.DirectionalLightComponent)
mc.set_editor_property("intensity", 3.0)
mc.set_editor_property("light_color", unreal.Color(r=150, g=175, b=255, a=255))
mc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)

sky = native_actor(unreal.SkyLight, (5000, 4000, 2800), 0, "SkyLight")
sc = sky.get_component_by_class(unreal.SkyLightComponent)
sc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
sc.set_editor_property("real_time_capture", False)
sc.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
sc.set_editor_property("cubemap", eal.load_asset("/Engine/MapTemplates/Sky/SunsetAmbientCubemap"))
sc.set_editor_property("light_color", unreal.Color(r=200, g=210, b=255, a=255))
sc.set_editor_property("intensity", 0.35)


def point(loc, lumens, color, radius, name, source=60.0):
    pl = native_actor(unreal.PointLight, loc, 0, name)
    c = pl.get_component_by_class(unreal.PointLightComponent)
    c.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
    c.set_editor_property("intensity", lumens)
    c.set_editor_property("light_color", color)
    c.set_editor_property("attenuation_radius", radius)
    c.set_editor_property("source_radius", source)
    c.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    return pl


WARM = unreal.Color(r=255, g=200, b=140, a=255)
mesh_actor(PROPS + "SM_CeilingLamp", (800, 5300, 3150), folder="Lighting")
point((800, 5300, 2350), 30000.0, WARM, 4000, "Lamp_Nook")
mesh_actor(PROPS + "SM_CeilingLamp", (5800, 4200, 3150), folder="Lighting")
point((5800, 4200, 2350), 25000.0, WARM, 4500, "Lamp_Center")
point((2700, 2450, 1000), 6500.0, unreal.Color(r=255, g=214, b=160, a=255), 1600, "TableKey", source=25.0)
point((3050, 1750, 820), 2500.0, unreal.Color(r=150, g=180, b=255, a=255), 1000, "MoonRim", source=20.0)

fog = native_actor(unreal.ExponentialHeightFog, (5000, 4000, 0), 0, "HeightFog")
fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fc.set_editor_property("fog_density", 0.004)
fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.08, 0.1, 0.18, 1.0))

ppv = native_actor(unreal.PostProcessVolume, (5000, 4000, 1000), 0, "PostProcess")
ppv.set_editor_property("unbound", True)
s = ppv.get_editor_property("settings")
for prop, value in [("auto_exposure_bias", 0.6), ("auto_exposure_min_brightness", 0.6), ("auto_exposure_max_brightness", 2.0),
                    ("bloom_intensity", 0.8), ("vignette_intensity", 0.55), ("white_temp", 5600.0),
                    ("depth_of_field_focal_distance", 370.0), ("depth_of_field_fstop", 2.0)]:
    s.set_editor_property("override_" + prop, True)
    s.set_editor_property(prop, value)
s.set_editor_property("override_color_saturation", True)
s.set_editor_property("color_saturation", unreal.Vector4(1.12, 1.08, 1.0, 1.0))
ppv.set_editor_property("settings", s)

# ------------------------------------------------------------------ menu camera
cam = native_actor(unreal.CameraActor, CAM_LOC, CAM_YAW, "MenuCamera", "Camera", pitch=CAM_PITCH)
cam.tags = ["MenuCamera"]
cc = cam.get_component_by_class(unreal.CameraComponent)
cc.set_editor_property("field_of_view", 70.0)
cc.set_editor_property("constrain_aspect_ratio", False)

# ------------------------------------------------------------------ world settings
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
ws = world.get_world_settings()
ws.set_editor_property("default_game_mode", unreal.MenuGameMode.static_class())
print("level saved", unreal.EditorLoadingAndSavingUtils.save_map(world, MAP), MAP)
