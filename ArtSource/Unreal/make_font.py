"""Create /Game/Crank/UI/F_Crank: Roboto with a Hangul fallback (DroidSansFallback, Apache 2.0) so Korean text
renders the same in packaged builds as in the editor."""
import unreal

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.CrankEditorLibrary
DIR = "/Game/Crank/UI"
FACE = DIR + "/FF_DroidSansFallback"
FONT = DIR + "/F_Crank"
ROBOTO = "/Engine/EngineFonts/Faces/RobotoRegular"

if not eal.does_asset_exist(FACE):
    task = unreal.AssetImportTask()
    task.filename = r"C:\Program Files\Epic Games\UE_5.8\Engine\Content\Slate\Fonts\DroidSansFallback.ttf"
    task.destination_path = DIR
    task.destination_name = "FF_DroidSansFallback"
    task.automated = True
    task.save = True
    task.replace_existing = True
    tools.import_asset_tasks([task])
face = eal.load_asset(FACE)
print("face", face, "roboto", eal.does_asset_exist(ROBOTO))

font = eal.load_asset(FONT) if eal.does_asset_exist(FONT) else tools.create_asset("F_Crank", DIR, unreal.Font, unreal.FontFactory())
font.set_editor_property("font_cache_type", unreal.FontCacheType.RUNTIME)
default_face = ROBOTO + ".RobotoRegular" if eal.does_asset_exist(ROBOTO) else FACE + ".FF_DroidSansFallback"
composite = (
    '(DefaultTypeface=(Fonts=((Name="Regular",Font=(FontFaceAsset="%s")))),'
    'FallbackTypeface=(Typeface=(Fonts=((Name="Regular",Font=(FontFaceAsset="%s"))))))'
    % (default_face, FACE + ".FF_DroidSansFallback")
)
print("set", lib.set_object_property_text(font, "CompositeFont", composite))
print(lib.get_object_property_text(font, "CompositeFont")[:400])
eal.save_loaded_asset(font)
