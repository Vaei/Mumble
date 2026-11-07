// Copyright (c) Jared Taylor

using UnrealBuildTool;

public class MumbleEditor : ModuleRules
{
	public MumbleEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"AssetTools",
				"MetasoundEngine",
				"MetasoundEditor",
				"MetasoundFrontend",
				"Mumble",
			}
		);
	}
}
