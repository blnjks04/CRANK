// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Crank : ModuleRules
{
	public Crank(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"PhysicsCore", "AIModule", "NetCore", "UMG", "Slate", "SlateCore", "DeveloperSettings", "RenderCore", "Sockets"
		});

		PublicIncludePaths.Add(ModuleDirectory);
	}
}
