"""Build /Game/Crank/Maps/Lvl_Kitchen (the clockmaker's kitchen raid)."""
import unreal

eal = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
MAP = "/Game/Crank/Maps/Lvl_Kitchen"
BP = "/Game/Crank/Blueprints/"
ENV = "/Game/Crank/Environment/"
PROPS = "/Game/Crank/Props/"


def V(x, y, z):
    return unreal.Vector(x, y, z)


def R(yaw=0.0, pitch=0.0, roll=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def label(a, name, folder=None):
    a.set_actor_label(name)
    if folder:
        a.set_folder_path(folder)
    return a


def mesh_actor(mesh, loc=(0, 0, 0), yaw=0.0, name=None, folder="Environment", tags=None, scale=None, pitch=0.0):
    m = eal.load_asset(mesh)
    a = actors.spawn_actor_from_object(m, V(*loc), R(yaw, pitch))
    label(a, name or mesh.rsplit("/", 1)[-1], folder)
    if scale:
        a.set_actor_scale3d(V(*scale))
    if tags:
        smc = a.get_component_by_class(unreal.StaticMeshComponent)
        smc.set_editor_property("component_tags", tags)
    return a


def bp_actor(bp_name, loc, yaw=0.0, name=None, folder="Gameplay", pitch=0.0):
    cls = eal.load_blueprint_class(BP + bp_name)
    a = actors.spawn_actor_from_class(cls, V(*loc), R(yaw, pitch))
    return label(a, name or bp_name, folder)


def native_actor(cls, loc, yaw=0.0, name=None, folder="Lighting", pitch=0.0):
    a = actors.spawn_actor_from_class(cls, V(*loc), R(yaw, pitch))
    return label(a, name or cls.__name__, folder)


# ------------------------------------------------------------------ new level
# start from an empty untitled world (this also unloads MAP if it is open), save over MAP at the end
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
if eal.does_asset_exist(MAP):
    eal.delete_asset(MAP)

# ------------------------------------------------------------------ environment (world-space meshes)
# Furniture built flush against the room walls keeps 1 cm off them: coplanar faces of two meshes z-fight (flicker).
FLUSH_OFFSET = {"SM_SinkCabinet": (0, -1, 0), "SM_CabinetClutter": (0, -1, 0), "SM_Counter": (-1, -1, 0),
                "SM_Fridge": (-1, 0, 0), "SM_Bookcase": (1, 0, 0),
                "SM_Wall_East": (0, 0, 0.2)}	# its crown moulding meets the north wall's at the corner
for m in ["SM_Floor", "SM_Wall_North", "SM_Wall_South", "SM_Wall_East", "SM_Wall_West", "SM_Ceiling", "SM_Bookcase",
          "SM_CrateStack", "SM_Workbench", "SM_RulerRamp", "SM_EntryWedge", "SM_SinkCabinet", "SM_CabinetClutter",
          "SM_Counter", "SM_Fridge", "SM_TableTop"]:
    mesh_actor(ENV + m, FLUSH_OFFSET.get(m, (0, 0, 0)))

mesh_actor(ENV + "SM_FridgeDoor_L", (8999, 800, 0), yaw=100)
mesh_actor(ENV + "SM_FridgeDoor_R", (8999, 2800, 0), yaw=-60)
for i, (x, y) in enumerate([(1950, 650), (4150, 650), (1950, 2850), (4150, 2850)]):
    # 5 mm lower: the leg caps would otherwise share the plane of the table top's underside
    mesh_actor(ENV + "SM_TableLeg", (x, y, -0.5), name="TableLeg_%d" % i, tags=["TableLeg", "GrabAnchor"])
for i, (x, y, yaw) in enumerate([(2600, 3060, -90), (3550, 3060, -90), (2600, 440, 90), (3550, 440, 90)]):
    mesh_actor(ENV + "SM_Chair", (x, y, 0), yaw=yaw, name="Chair_%d" % i, tags=["GrabAnchor"])

# nook decor
mesh_actor(PROPS + "SM_Rug", (800, 6050, 0), folder="Decor")
mesh_actor(PROPS + "SM_Nightlight", (300, 7600, 0), folder="Decor")
# barriers around the return path (south-west region)
# The crates overlap each other / the crate stack: every other one 1% bigger and all 3 mm up so no faces coincide.
for i, (x, y) in enumerate([(1650, 150), (1650, 450), (1650, 3050), (1650, 3300)]):
    sc = 1.0 if i % 2 == 0 else 1.01
    mesh_actor(PROPS + "SM_Crate", (x, y, 0.3), folder="Barriers", scale=(sc, sc, sc))
# can fences: canyon/pocket divider (X = 7800) and the long south fence (Y = 4800)
for k in range(3):
    # crosses the south fence: sunk 1.6 cm into the floor so its can tops / rims never share a plane with it
    mesh_actor(PROPS + "SM_CanFence", (7800, 4800 + k * 600, -1.6), yaw=90, name="Fence_Pocket_%d" % k, folder="Barriers")
x = 1900
k = 0
while x < 10000:
    mesh_actor(PROPS + "SM_CanFence", (x, 4800, 0), name="Fence_South_%d" % k, folder="Barriers")
    x += 600
    k += 1

# ------------------------------------------------------------------ checkpoints
def checkpoint(zone, order, name_kr, loc, yaw, extent=(300, 300, 200)):
    cp = bp_actor("BP_Checkpoint", loc, yaw, "CP_" + zone, "Checkpoints")
    cp.set_editor_property("zone_id", zone)
    cp.set_editor_property("order", order)
    cp.set_editor_property("zone_name", name_kr)
    box = cp.get_component_by_class(unreal.BoxComponent)
    box.set_box_extent(V(*extent))
    return cp

checkpoint("Start", 0, "작업대 — 출발", (850, 4650, 340), 90, (350, 350, 150))
checkpoint("A", 1, "A. 싱크대 수납장 통로", (2150, 6950, 150), 0, (300, 400, 200))
checkpoint("B", 2, "B. 타일 바닥과 냄비 협곡", (5150, 5940, 150), 0, (300, 300, 200))
checkpoint("C", 3, "C. 조리대 등반", (8800, 5500, 100), 90, (900, 600, 200))
checkpoint("D", 4, "D. 냉장고 앞 (추위: 소모 x1.5)", (9300, 3500, 100), 180, (500, 500, 200))
checkpoint("E", 5, "E. 식탁 아래 — 먼지먹개 MK-II", (4700, 1750, 100), 180, (350, 900, 200))
checkpoint("Return", 6, "귀환 — 식탁보 숏컷", (900, 2000, 100), 90, (700, 800, 200))
native_actor(unreal.PlayerStart, (850, 4650, 400), 90, "PlayerStart", "Checkpoints")

# ------------------------------------------------------------------ B: pot canyon
mesh_actor(PROPS + "SM_BookStack", (5150, 5940, 0), name="B_P0_Books", folder="B")
mesh_actor(PROPS + "SM_Pot_A", (6360, 5850, 0), name="B_Pot1", folder="B")
mesh_actor(PROPS + "SM_Pot_B", (7450, 5650, 0), name="B_Pot2", folder="B")
mesh_actor(PROPS + "SM_Pot_C", (6000, 5150, 0), name="B_Pot3_Gear", folder="B")
mesh_actor(PROPS + "SM_TrayRamp", (5150, 4950, 0), yaw=90, name="B_DetourRamp", folder="B")
bridge1 = bp_actor("BP_Bridge_Spatula", (6110, 5850, 262), 180, "B_Bridge1", "B")
bridge1.set_editor_property("open_rotation", R(0, -15))
bridge1.set_editor_property("closed_rotation", R(0, 80))
lever1 = bp_actor("BP_PullLever", (6450, 5850, 260), 180, "B_Lever1", "B")
lever1.set_editor_property("targets", [bridge1])
bridge2 = bp_actor("BP_Bridge_Spatula", (7195, 5690, 342), 169.6, "B_Bridge2", "B")
bridge2.set_editor_property("open_rotation", R(0, -8))
bridge2.set_editor_property("closed_rotation", R(0, 80))
lever2 = bp_actor("BP_PullLever", (7560, 5650, 340), 180, "B_Lever2", "B")
lever2.set_editor_property("targets", [bridge2])
bp_actor("BP_Part_Gear", (6000, 5150, 490), 0, "Part_Gear", "B")
tube_gear = bp_actor("BP_DeliveryTube", (4950, 6130, 100), 0, "Tube_Gear", "B")
tube_gear.set_editor_property("accepted_parts", [unreal.CrankPartType.GEAR])
bp_actor("BP_WindStation", (5300, 5790, 100), 0, "Station_B", "B")

# ------------------------------------------------------------------ C: counter
extents = [380, 300, 220, 150, 90]
for k in range(5):
    d = bp_actor("BP_PullDrawer", (8400, 6300, 5 + k * 100), -90, "C_Drawer_%d" % k, "C")
    d.set_editor_property("max_extent", float(extents[k]))
bp_actor("BP_Part_SpringCoil", (9600, 5250, 60), 0, "Part_SpringCoil", "C")
tube_coil = bp_actor("BP_DeliveryTube", (8200, 7700, 600), 0, "Tube_Coil", "C")
tube_coil.set_editor_property("accepted_parts", [unreal.CrankPartType.SPRING_COIL])
bp_actor("BP_WindStation", (8450, 7000, 600), 0, "Station_C", "C")
mesh_actor(PROPS + "SM_Toaster", (9050, 6750, 600), yaw=15, folder="C")
mesh_actor(PROPS + "SM_Kettle", (9700, 6650, 600), yaw=-30, folder="C")
mesh_actor(PROPS + "SM_CoolingRack", (9450, 7350, 600), name="C_CrawlRack", folder="C")
mesh_actor(PROPS + "SM_BreadBox", (9450.5, 7350, 662), name="C_BreadBox", folder="C")
# neighbouring jars (and the rack) touch: tiny height steps keep their base plates out of each other's plane
for i, (jx, jy, jm, jz) in enumerate([(8950, 7740, "SM_Jar_A", 0.0), (8950, 7905, "SM_Jar_B", 0.3), (9925, 7640, "SM_Jar_B", 0.6), (9925, 7860, "SM_Jar_B", 0.3)]):
    mesh_actor(PROPS + jm, (jx, jy, 600 + jz), name="C_Jar_%d" % i, folder="C")
board = bp_actor("BP_Bridge_Board", (9400, 6240, 600), -90, "C_BoardRamp", "C")
board.set_editor_property("open_rotation", R(0, -13.3))
board.set_editor_property("closed_rotation", R(0, 80))
lever3 = bp_actor("BP_PullLever", (9450, 7850, 600), 180, "C_CrawlLever", "C")
lever3.set_editor_property("targets", [board])
lever3.set_editor_property("label", "레버 당기기 (판자 다리 내리기)")

# ------------------------------------------------------------------ D: fridge front
cold = bp_actor("BP_SurfaceZone", (7700, 2700, 60), 0, "D_ColdZone", "D")
cold.set_editor_property("drain_multiplier", 1.5)
cold.set_editor_property("cold_mist", True)
cold.set_editor_property("zone_label", "차가운 바닥")
cold.get_component_by_class(unreal.BoxComponent).set_box_extent(V(1300, 1900, 100))
bp_actor("BP_WindStation", (7300, 3250, 0), 0, "Station_D", "D")
bp_actor("BP_Part_JewelBearing", (9450, 2350, 140), 0, "Part_JewelBearing", "D")
tube_bearing = bp_actor("BP_DeliveryTube", (5900, 2300, 0), 0, "Tube_Bearing", "D")
tube_bearing.set_editor_property("accepted_parts", [unreal.CrankPartType.JEWEL_BEARING])

# ------------------------------------------------------------------ E: boss arena
arena = bp_actor("BP_BossArena", (3050, 1750, 0), 0, "BossArena", "E")
dock = bp_actor("BP_ChargingDock", (2150, 2700, 0), 0, "ChargingDock", "E")
boss = bp_actor("BP_Boss_DustEater", (3050, 1600, 0), 0, "Boss_DustEater", "E")
boss.set_editor_property("dock", dock)
boss.set_editor_property("arena", arena)
boss.set_editor_property("patrol_points", [V(2450, 1150, 0), V(3650, 1150, 0), V(3650, 2350, 0), V(2450, 2350, 0)])
arena.set_editor_property("boss", boss)
bp_actor("BP_WindStation", (4000, 900, 0), 0, "Station_E", "E")
bp_actor("BP_Dishcloth", (3150, 850, 40), 0, "Dishcloth", "E")
shortcut = bp_actor("BP_TableclothShortcut", (1790, 1750, 650), 0, "TableclothShortcut", "E")
arena.set_editor_property("open_on_defeat", [shortcut])
slide = bp_actor("BP_SurfaceZone", (1200, 1750, 60), 0, "Return_SlideZone", "Return")
slide.set_editor_property("speed_boost", 1.35)
slide.set_editor_property("ground_friction", 2.0)
slide.get_component_by_class(unreal.BoxComponent).set_box_extent(V(500, 1300, 100))
bp_actor("BP_RaidClock", (250, 1600, 0), 0, "GrandfatherClock", "Decor")

# ------------------------------------------------------------------ nook: delivery of the main spring + golden key
wb = bp_actor("BP_DeliveryWorkbench", (480, 4430, 300.5), 0, "Workbench_Delivery", "Start")	# 5 mm above the bench top
wb.set_editor_property("accepted_parts", [unreal.CrankPartType.MAIN_SPRING, unreal.CrankPartType.GOLDEN_KEY])

# ------------------------------------------------------------------ F: toy room behind the mouse hole (giant cymbal monkey)
for m in ["SM_Toy_Floor", "SM_Toy_Walls", "SM_Toy_Ceiling"]:
    mesh_actor(ENV + m, (0, 1, 0) if m == "SM_Toy_Ceiling" else (0, 0, 0), folder="F")	# ceiling trim off the wall plane
mesh_actor(PROPS + "SM_Toy_Rug", (3000, 11100, 0.3), folder="F")	# 3 mm above the floor boards
mesh_actor(PROPS + "SM_Toy_Chest", (5150, 13450, 0), yaw=-12, folder="F")
mesh_actor(PROPS + "SM_Toy_Ball", (750, 13350, 0), folder="F")
mesh_actor(PROPS + "SM_Toy_Books", (5350, 8800, 0), yaw=20, folder="F")
# block steps: a small block next to a big one = high ground the floor shockwaves cannot reach
center = unreal.Vector(3000, 11100, 0)
for i, (x, y, yaw) in enumerate([(1250, 10300, 10), (4800, 9900, -20), (1900, 13100, 35), (4500, 12700, 0)]):
    mesh_actor(PROPS + "SM_Toy_BlockBig", (x, y, 0), yaw=yaw, name="F_BlockBig_%d" % i, folder="F")
    d = unreal.Vector(center.x - x, center.y - y, 0)
    d = d * (1.0 / max(1.0, (d.x ** 2 + d.y ** 2) ** 0.5))
    mesh_actor(PROPS + "SM_Toy_BlockSmall", (x + d.x * 155, y + d.y * 155, 0.3), yaw=yaw + 15, name="F_BlockStep_%d" % i, folder="F")
checkpoint("F", 7, "F. 장난감 방 — 짝짝이 대왕", (650, 8650, 150), 90, (420, 380, 200))
bp_actor("BP_WindStation", (1750, 8700, 0), 0, "Station_F1", "F")
bp_actor("BP_WindStation", (5250, 11900, 0), 0, "Station_F2", "F")
giant_arena = bp_actor("BP_GiantArena", (3000, 11100, 0), 0, "GiantArena", "F")
giant = bp_actor("BP_Boss_GiantMonkey", (3000, 11800, 452), -90, "Boss_GiantMonkey", "F")
giant.set_editor_property("arena", giant_arena)
giant_arena.set_editor_property("boss", giant)

# ------------------------------------------------------------------ lighting
moon = native_actor(unreal.DirectionalLight, (3000, -500, 2500), 70, "MoonLight", pitch=-32)
mc = moon.get_component_by_class(unreal.DirectionalLightComponent)
mc.set_editor_property("intensity", 3.0)
mc.set_editor_property("light_color", unreal.Color(r=150, g=175, b=255, a=255))
mc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)

sky = native_actor(unreal.SkyLight, (5000, 4000, 2800), 0, "SkyLight")
sc = sky.get_component_by_class(unreal.SkyLightComponent)
sc.set_editor_property("intensity", 1.0)
sc.set_editor_property("light_color", unreal.Color(r=200, g=210, b=255, a=255))
sc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
sc.set_editor_property("real_time_capture", False)
sc.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
cube = eal.load_asset("/Engine/MapTemplates/Sky/SunsetAmbientCubemap") or eal.load_asset("/Engine/EngineResources/DefaultTextureCube")
if cube:
    sc.set_editor_property("cubemap", cube)
sc.set_editor_property("intensity", 0.35)


def lamp(loc, intensity, color, radius, name, mesh=True):
    if mesh:
        mesh_actor(PROPS + "SM_CeilingLamp", (loc[0], loc[1], 3150), name=name + "_Fixture", folder="Lighting")
    pl = native_actor(unreal.PointLight, loc, 0, name)
    c = pl.get_component_by_class(unreal.PointLightComponent)
    c.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
    c.set_editor_property("intensity", intensity)
    c.set_editor_property("light_color", color)
    c.set_editor_property("attenuation_radius", radius)
    c.set_editor_property("source_radius", 60.0)
    c.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    return pl


WARM = unreal.Color(r=255, g=200, b=140, a=255)
COOL = unreal.Color(r=190, g=225, b=255, a=255)
lamp((3050, 1750, 2350), 60000.0, WARM, 5500, "Lamp_Table")
lamp((8900, 6900, 2350), 40000.0, WARM, 4500, "Lamp_Counter")
lamp((800, 5300, 2350), 30000.0, WARM, 4000, "Lamp_Nook")
lamp((5800, 4200, 2350), 25000.0, WARM, 4500, "Lamp_Center")
lamp((9450, 1800, 1500), 20000.0, COOL, 3000, "Light_Fridge", mesh=False)
lamp((3000, 11100, 2600), 55000.0, WARM, 7000, "Lamp_ToyRoom")
lamp((3000, 13600, 1900), 9000.0, COOL, 3500, "Light_ToyMoon", mesh=False)
lamp((600, 8350, 260), 2500.0, unreal.Color(r=255, g=190, b=110, a=255), 1300, "Light_MouseHole", mesh=False)
lamp((8300, 2000, 300), 6000.0, COOL, 2500, "Light_FridgeSpill", mesh=False)
lamp((3100, 7650, 300), 2500.0, unreal.Color(r=255, g=190, b=110, a=255), 1600, "Light_Cabinet1", mesh=False)
lamp((4600, 6650, 300), 2500.0, unreal.Color(r=255, g=190, b=110, a=255), 1600, "Light_Cabinet2", mesh=False)
mesh_actor(PROPS + "SM_Nightlight", (3100, 7650, 100), folder="Decor")
mesh_actor(PROPS + "SM_Nightlight", (4600, 6650, 100), folder="Decor")

fog = native_actor(unreal.ExponentialHeightFog, (5000, 4000, 0), 0, "HeightFog")
fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fc.set_editor_property("fog_density", 0.004)
fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.08, 0.1, 0.18, 1.0))

ppv = native_actor(unreal.PostProcessVolume, (5000, 4000, 1000), 0, "PostProcess")
ppv.set_editor_property("unbound", True)
s = ppv.get_editor_property("settings")
s.set_editor_property("override_auto_exposure_bias", True)
s.set_editor_property("auto_exposure_bias", 0.6)
s.set_editor_property("override_auto_exposure_min_brightness", True)
s.set_editor_property("auto_exposure_min_brightness", 0.6)
s.set_editor_property("override_auto_exposure_max_brightness", True)
s.set_editor_property("auto_exposure_max_brightness", 2.0)
s.set_editor_property("override_bloom_intensity", True)
s.set_editor_property("bloom_intensity", 0.8)
s.set_editor_property("override_vignette_intensity", True)
s.set_editor_property("vignette_intensity", 0.45)
s.set_editor_property("override_color_saturation", True)
s.set_editor_property("color_saturation", unreal.Vector4(1.12, 1.08, 1.0, 1.0))
s.set_editor_property("override_white_temp", True)
s.set_editor_property("white_temp", 5600.0)
ppv.set_editor_property("settings", s)

# ------------------------------------------------------------------ world settings
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
ws = world.get_world_settings()
ws.set_editor_property("default_game_mode", eal.load_blueprint_class(BP + "BP_RaidGameMode"))
ws.set_editor_property("kill_z", -800.0)

print("level saved", unreal.EditorLoadingAndSavingUtils.save_map(world, MAP), MAP)
