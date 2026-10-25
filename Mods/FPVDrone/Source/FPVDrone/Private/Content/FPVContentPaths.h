// All asset paths in one place.
//
// Mod assets follow Assets/Docs/ASSETS.md (mount point /FPVDrone/). Every asset is optional:
// the code falls back to engine shapes or silence when something is missing.
//
// FactoryGame paths are from the game's content and MUST be checked in the editor after
// a game update (Content Browser -> right click -> Copy Reference).

#pragma once

namespace FPVContentPaths
{
	// --- Mod: meshes -----------------------------------------------------------
	constexpr const TCHAR* FrameMesh = TEXT("/FPVDrone/Drone/Mesh/SM_FPVDrone_Frame.SM_FPVDrone_Frame");
	constexpr const TCHAR* PropCWMesh = TEXT("/FPVDrone/Drone/Mesh/SM_FPVDrone_Prop_CW.SM_FPVDrone_Prop_CW");
	constexpr const TCHAR* PropCCWMesh = TEXT("/FPVDrone/Drone/Mesh/SM_FPVDrone_Prop_CCW.SM_FPVDrone_Prop_CCW");
	constexpr const TCHAR* PropBlurMesh = TEXT("/FPVDrone/Drone/Mesh/SM_FPVDrone_PropBlur.SM_FPVDrone_PropBlur");
	constexpr const TCHAR* CameraMesh = TEXT("/FPVDrone/Drone/Mesh/SM_FPVDrone_Camera.SM_FPVDrone_Camera");

	// Fallbacks (engine content)
	constexpr const TCHAR* FallbackCube = TEXT("/Engine/BasicShapes/Cube.Cube");
	constexpr const TCHAR* FallbackCylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

	// Sockets on the frame mesh
	constexpr const TCHAR* SocketProp[4] = { TEXT("Socket_Prop_FL"), TEXT("Socket_Prop_FR"), TEXT("Socket_Prop_RL"), TEXT("Socket_Prop_RR") };
	constexpr const TCHAR* SocketCamera = TEXT("Socket_FPVCam");

	// --- Mod: icons, UI --------------------------------------------------------
	constexpr const TCHAR* IconBig = TEXT("/FPVDrone/Drone/Icons/IconFPVDrone_256.IconFPVDrone_256");
	constexpr const TCHAR* IconSmall = TEXT("/FPVDrone/Drone/Icons/IconFPVDrone_64.IconFPVDrone_64");
	constexpr const TCHAR* OSDVignette = TEXT("/FPVDrone/UI/OSD/T_OSD_GogglesVignette.T_OSD_GogglesVignette");
	constexpr const TCHAR* OSDStatic = TEXT("/FPVDrone/UI/OSD/T_OSD_StaticNoise.T_OSD_StaticNoise");

	// --- Mod: sounds -----------------------------------------------------------
	constexpr const TCHAR* SoundMotorLoop = TEXT("/FPVDrone/Audio/SW_Motor_Loop.SW_Motor_Loop");
	constexpr const TCHAR* SoundPropwashLoop = TEXT("/FPVDrone/Audio/SW_PropWash_Loop.SW_PropWash_Loop");
	constexpr const TCHAR* SoundArm = TEXT("/FPVDrone/Audio/SW_Arm.SW_Arm");
	constexpr const TCHAR* SoundDisarm = TEXT("/FPVDrone/Audio/SW_Disarm.SW_Disarm");
	constexpr const TCHAR* SoundCrash = TEXT("/FPVDrone/Audio/SW_Crash.SW_Crash");
	constexpr const TCHAR* SoundModeSwitch = TEXT("/FPVDrone/Audio/SW_ModeSwitch.SW_ModeSwitch");
	constexpr const TCHAR* SoundWarnBeep = TEXT("/FPVDrone/Audio/SW_Beep_Warn.SW_Beep_Warn");

	// --- FactoryGame (verify!) -------------------------------------------------
	constexpr const TCHAR* ItemCircuitBoard = TEXT("/Game/FactoryGame/Resource/Parts/CircuitBoard/Desc_CircuitBoard.Desc_CircuitBoard_C");
	constexpr const TCHAR* ItemQuickwire = TEXT("/Game/FactoryGame/Resource/Parts/HighSpeedWire/Desc_HighSpeedWire.Desc_HighSpeedWire_C");
	constexpr const TCHAR* ItemRotor = TEXT("/Game/FactoryGame/Resource/Parts/Rotor/Desc_Rotor.Desc_Rotor_C");
	constexpr const TCHAR* ItemRubber = TEXT("/Game/FactoryGame/Resource/Parts/Rubber/Desc_Rubber.Desc_Rubber_C");
	constexpr const TCHAR* ItemCoupon = TEXT("/Game/FactoryGame/Resource/Parts/ResourceSinkCoupon/Desc_ResourceSinkCoupon.Desc_ResourceSinkCoupon_C");
	constexpr const TCHAR* EquipmentWorkshop = TEXT("/Game/FactoryGame/Buildable/-Shared/WorkBench/BP_WorkshopComponent.BP_WorkshopComponent_C");
	constexpr const TCHAR* CraftBench = TEXT("/Game/FactoryGame/Buildable/-Shared/WorkBench/BP_WorkBenchComponent.BP_WorkBenchComponent_C");
}
