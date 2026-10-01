import os
import unreal

SRC = r"D:\Game\CRANK\ArtSource\Audio\wav"
DEST = "/Game/Crank/Audio"
LOOPS = {"SFX_MusicBox", "SFX_VacuumLoop", "SFX_SuctionLoop"}
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

tasks = []
for f in sorted(os.listdir(SRC)):
    if not f.endswith(".wav"):
        continue
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", os.path.join(SRC, f))
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", os.path.splitext(f)[0])
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tasks.append(t)
tools.import_asset_tasks(tasks)

for name in LOOPS:
    s = eal.load_asset(DEST + "/" + name)
    if s:
        s.set_editor_property("looping", True)
        eal.save_loaded_asset(s)
print(len(eal.list_assets(DEST, recursive=False)), "sounds")
