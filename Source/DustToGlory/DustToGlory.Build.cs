using UnrealBuildTool;

public class DustToGlory : ModuleRules
{
	public DustToGlory(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets every file include by path from the module root, e.g. "Sim/Economy.h" or "Game/DTGVehicle.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"PhysicsCore"
		});
	}
}
