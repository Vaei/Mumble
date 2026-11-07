// Copyright (c) Jared Taylor

using UnrealBuildTool;

public class Mumble : ModuleRules
{
	public Mumble(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"DeveloperSettings",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"AudioExtensions",
				"MetasoundGraphCore",
				"MetasoundFrontend",
				"MetasoundEngine",
				"MetasoundStandardNodes",
			}
			);

		// Registers the node in this module's own list rather than the global fallback
		PrivateDefinitions.Add("METASOUND_PLUGIN=Mumble");
		PrivateDefinitions.Add("METASOUND_MODULE=Mumble");
	}
}
