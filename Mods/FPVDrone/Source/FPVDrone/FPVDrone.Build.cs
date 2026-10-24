using UnrealBuildTool;

public class FPVDrone : ModuleRules
{
	public FPVDrone(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "ApplicationCore",
			"Slate", "SlateCore", "UMG", "Json", "JsonUtilities",
			"NetCore", "EnhancedInput", "GameplayTags", "DeveloperSettings",
		});

		// Satisfactory + SML
		PublicDependencyModuleNames.AddRange(new string[] { "FactoryGame", "SML" });

		// Raw HID access for RC transmitters in joystick mode (client only).
		if (Target.Platform == UnrealTargetPlatform.Win64 && Target.Type != TargetType.Server)
		{
			PublicSystemLibraries.AddRange(new string[] { "hid.lib", "setupapi.lib" });
			PublicDefinitions.Add("FPV_WITH_HID=1");
		}
		else
		{
			PublicDefinitions.Add("FPV_WITH_HID=0");
		}
	}
}
