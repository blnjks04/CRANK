using UnrealBuildTool;

public class CrankEditor : ModuleRules
{
	public CrankEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd", "PhysicsUtilities", "AssetTools", "AssetRegistry", "Kismet", "BlueprintGraph", "Crank", "PhysicsCore"
		});
	}
}
