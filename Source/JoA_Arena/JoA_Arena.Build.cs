// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class JoA_Arena : ModuleRules
{
	public JoA_Arena(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine",
			"Niagara",
			"DeveloperSettings",
			"NetCore",
			"CoreOnline",
			"InputCore",
			"EnhancedInput",
			"UMG",
			// GAS
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			// Iris Replication
			"IrisCore"
		});

		// Iris Replikace - activation
		SetupIrisSupport(Target);

		// LogJoA_Net is declared in JoA_Arena.h, at the module root.
		PrivateIncludePaths.Add(ModuleDirectory);

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
			"ProceduralMeshComponent",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"OnlineSubsystemSteam"
		});

		// Automation tests start a PIE session.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
