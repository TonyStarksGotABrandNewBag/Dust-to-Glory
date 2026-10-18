using UnrealBuildTool;

public class DustToGlory : ModuleRules
{
	public DustToGlory(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// The simulation is C++20 by contract (see Source/DustToGlory/Sim), so say so rather than
		// relying on whichever default the engine ships with.
		CppStandard = CppStandardVersion.Cpp20;

		// Unity builds concatenate source files, and a file-scope "using namespace dtg::sim" then
		// leaks into whichever files happen to follow it in the blob. dtg::sim exports short,
		// ordinary names -- Rect, Message, Profile, Market, Clock, Crime, Verdict, Alignment -- so a
		// name in a file that never asked for them could quietly resolve to one, and differently
		// depending on how the files were grouped that day. A module this size builds fast enough
		// that order-independence is worth more than the batching.
		bUseUnity = false;

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
			"PhysicsCore",
			// Terrain, roads and water are generated at runtime from the simulation's state plan.
			"ProceduralMeshComponent",
			// The engine note is synthesised per sample (see Game/DTGEngineAudio).
			"AudioMixer",
			"SignalProcessing",
			// The HUD and menus are drawn in code; no UMG assets to maintain.
			"UMG",
			"Slate",
			"SlateCore"
		});
	}
}
