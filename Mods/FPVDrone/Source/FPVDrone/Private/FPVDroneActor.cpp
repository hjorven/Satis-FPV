#include "FPVDroneActor.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Content/FPVContentPaths.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "FGCharacterPlayer.h"
#include "FGHUD.h"
#include "UI/FGGameUI.h"
#include "Blueprint/UserWidget.h"
#include "FPVPilotLink.h"
#include "FPVDroneModule.h"
#include "FPVSettings.h"
#include "GameFramework/PlayerController.h"
#include "Input/FPVInputSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Power/FPVPowerGrid.h"
#include "Sound/SoundBase.h"
#include "UI/SFPVOSD.h"

#define LOCTEXT_NAMESPACE "FPVDrone"

namespace FPVDroneActorLocal
{
	constexpr float SendInterval = 1.0f / 30.0f;
	constexpr float ServerSendInterval = 1.0f / 20.0f;
	constexpr float InterpolationDelay = 0.1f;
	constexpr float MaxServerSpeedCm = 20000.0f;	// 200 m/s, far above anything the sim can do
	constexpr float CollisionRadiusCm = 13.0f;
	constexpr float VisualPropRevPerSec = 45.0f;
	constexpr float BlurThreshold = 0.3f;

	// Motor positions in the Unreal frame (X forward, Y right), FL, FR, RL, RR.
	const FVector2D MotorDir[4] = { { 1, -1 }, { 1, 1 }, { -1, -1 }, { -1, 1 } };
	// CW props spin with positive yaw in Unreal (seen from above), CCW negative. FL/RR are CCW.
	constexpr float MotorVisualSign[4] = { -1, 1, 1, -1 };

	template <typename T>
	T* LoadOptional(const TCHAR* Path)
	{
		return LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	FVector DirToWorld(const FPVSim::Vec3& V) { return FVector(V.X, -V.Y, V.Z); }
	FPVSim::Vec3 DirToSim(const FVector& V) { return FPVSim::Vec3(static_cast<float>(V.X), static_cast<float>(-V.Y), static_cast<float>(V.Z)); }
	// Angular velocity is an axial vector: mirroring Y flips the X and Z components instead.
	FVector AxialToWorld(const FPVSim::Vec3& V) { return FVector(-V.X, V.Y, -V.Z); }
	FPVSim::Vec3 AxialToSim(const FVector& V) { return FPVSim::Vec3(static_cast<float>(-V.X), static_cast<float>(V.Y), static_cast<float>(-V.Z)); }
}

using namespace FPVDroneActorLocal;

AFPVDroneActor::AFPVDroneActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	bReplicates = true;
	SetReplicateMovement(false);
	NetUpdateFrequency = 60.0f;
	MinNetUpdateFrequency = 5.0f;
	NetCullDistanceSquared = FMath::Square(250000.0f);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(CollisionRadiusCm);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetCanEverAffectNavigation(false);
	RootComponent = Collision;

	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
	Frame->SetupAttachment(Collision);
	Frame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Frame->SetCanEverAffectNavigation(false);

	CameraPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraPivot"));
	CameraPivot->SetupAttachment(Frame);
	CameraPivot->SetRelativeLocation(FVector(8.0f, 0.0f, 2.5f));

	CameraBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CameraBody"));
	CameraBody->SetupAttachment(CameraPivot);
	CameraBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraPivot);
	Camera->bUsePawnControlRotation = false;
	Camera->bConstrainAspectRatio = false;
	Camera->SetFieldOfView(120.0f);

	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Prop = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Prop%d"), Index));
		Prop->SetupAttachment(Frame);
		Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Props.Add(Prop);

		UStaticMeshComponent* Blur = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PropBlur%d"), Index));
		Blur->SetupAttachment(Prop);
		Blur->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Blur->SetVisibility(false);
		PropBlurs.Add(Blur);
	}

	MotorAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("MotorAudio"));
	MotorAudio->SetupAttachment(Collision);
	MotorAudio->bAutoActivate = false;

	PropwashAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("PropwashAudio"));
	PropwashAudio->SetupAttachment(Collision);
	PropwashAudio->bAutoActivate = false;
}

void AFPVDroneActor::BeginPlay()
{
	Super::BeginPlay();

	SettingsChangedHandle = FFPVSettings::OnChanged.AddWeakLambda(this, [this]() { bParamsDirty = true; });
	RefreshParams();
	FPVSim::ChargeBattery(Params, Sim);

	if (GetNetMode() != NM_DedicatedServer)
	{
		LoadAssets();
		SetupVisuals();
	}

	// Remote copies start where the server put them.
	Sim.Pos = WorldToSim(GetActorLocation());
	Sim.Rot = WorldToSim(GetActorQuat());
}

void AFPVDroneActor::EndPlay(const EEndPlayReason::Type Reason)
{
	if (bLocallyPiloting)
	{
		EndLocalPiloting();
	}
	FFPVSettings::OnChanged.Remove(SettingsChangedHandle);
	Super::EndPlay(Reason);
}

void AFPVDroneActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFPVDroneActor, ReplicatedState);
	DOREPLIFETIME(AFPVDroneActor, bPiloted);
	DOREPLIFETIME(AFPVDroneActor, bChargerInRange);
}

void AFPVDroneActor::LoadAssets()
{
	SoundArm = LoadOptional<USoundBase>(FPVContentPaths::SoundArm);
	SoundDisarm = LoadOptional<USoundBase>(FPVContentPaths::SoundDisarm);
	SoundCrash = LoadOptional<USoundBase>(FPVContentPaths::SoundCrash);
	SoundModeSwitch = LoadOptional<USoundBase>(FPVContentPaths::SoundModeSwitch);
	SoundWarn = LoadOptional<USoundBase>(FPVContentPaths::SoundWarnBeep);
	TextureVignette = LoadOptional<UTexture2D>(FPVContentPaths::OSDVignette);
	TextureStatic = LoadOptional<UTexture2D>(FPVContentPaths::OSDStatic);
	MotorAudio->SetSound(LoadOptional<USoundBase>(FPVContentPaths::SoundMotorLoop));
	PropwashAudio->SetSound(LoadOptional<USoundBase>(FPVContentPaths::SoundPropwashLoop));
}

void AFPVDroneActor::SetupVisuals()
{
	if (UStaticMesh* FrameMesh = LoadOptional<UStaticMesh>(FPVContentPaths::FrameMesh))
	{
		Frame->SetStaticMesh(FrameMesh);
	}
	else if (UStaticMesh* Cube = LoadOptional<UStaticMesh>(FPVContentPaths::FallbackCube))
	{
		// Placeholder: flat diamond plate the size of a 5" frame.
		Frame->SetStaticMesh(Cube);
		Frame->SetRelativeScale3D(FVector(0.16f, 0.16f, 0.02f));
		Frame->SetRelativeRotation(FRotator(0.0f, 45.0f, 0.0f));
	}

	const bool bFrameHasSockets = Frame->DoesSocketExist(FPVContentPaths::SocketProp[0]);
	UStaticMesh* PropCW = LoadOptional<UStaticMesh>(FPVContentPaths::PropCWMesh);
	UStaticMesh* PropCCW = LoadOptional<UStaticMesh>(FPVContentPaths::PropCCWMesh);
	UStaticMesh* PropBlur = LoadOptional<UStaticMesh>(FPVContentPaths::PropBlurMesh);
	UStaticMesh* Cylinder = LoadOptional<UStaticMesh>(FPVContentPaths::FallbackCylinder);

	const float ArmCm = FFPVSettings::Get().Craft.ArmLengthM * 100.0f * 0.70710678f;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Prop = Props[Index];
		if (bFrameHasSockets)
		{
			Prop->AttachToComponent(Frame, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FPVContentPaths::SocketProp[Index]);
		}
		else
		{
			// The placeholder frame is scaled and rotated, keep props in drone space.
			Prop->AttachToComponent(Collision, FAttachmentTransformRules::KeepRelativeTransform);
			Prop->SetRelativeLocation(FVector(MotorDir[Index].X * ArmCm, MotorDir[Index].Y * ArmCm, 2.0f));
		}

		UStaticMesh* Mesh = MotorVisualSign[Index] > 0.0f ? PropCW : PropCCW;
		if (Mesh != nullptr)
		{
			Prop->SetStaticMesh(Mesh);
		}
		else if (Cylinder != nullptr)
		{
			Prop->SetStaticMesh(Cylinder);
			Prop->SetRelativeScale3D(FVector(0.127f, 0.127f, 0.004f));
		}
		if (PropBlur != nullptr)
		{
			PropBlurs[Index]->SetStaticMesh(PropBlur);
		}
	}

	if (Frame->DoesSocketExist(FPVContentPaths::SocketCamera))
	{
		CameraPivot->AttachToComponent(Frame, FAttachmentTransformRules::SnapToTargetNotIncludingScale, FPVContentPaths::SocketCamera);
	}
	else if (!Frame->GetStaticMesh() || Frame->GetRelativeScale3D() != FVector::OneVector)
	{
		CameraPivot->AttachToComponent(Collision, FAttachmentTransformRules::KeepRelativeTransform);
		CameraPivot->SetRelativeLocation(FVector(8.0f, 0.0f, 2.5f));
	}
	if (UStaticMesh* CamMesh = LoadOptional<UStaticMesh>(FPVContentPaths::CameraMesh))
	{
		CameraBody->SetStaticMesh(CamMesh);
	}
	ApplyCameraSettings();
}

// --- Coordinate conversion ----------------------------------------------------

FVector AFPVDroneActor::SimToWorld(const FPVSim::Vec3& V)
{
	return DirToWorld(V) * 100.0;
}

FPVSim::Vec3 AFPVDroneActor::WorldToSim(const FVector& V)
{
	return DirToSim(V) * 0.01f;
}

FQuat AFPVDroneActor::SimToWorld(const FPVSim::Quat& Q)
{
	// Mirror across the XZ plane: q' = (w, -x, y, -z).
	return FQuat(-Q.X, Q.Y, -Q.Z, Q.W);
}

FPVSim::Quat AFPVDroneActor::WorldToSim(const FQuat& Q)
{
	return FPVSim::Quat(static_cast<float>(Q.W), static_cast<float>(-Q.X), static_cast<float>(Q.Y), static_cast<float>(-Q.Z));
}

// --- Ownership / piloting -------------------------------------------------------

APlayerController* AFPVDroneActor::GetOwningPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

bool AFPVDroneActor::IsLocallyPiloted() const
{
	const APlayerController* PC = GetOwningPlayerController();
	return bPiloted && PC != nullptr && PC->IsLocalController();
}

void AFPVDroneActor::ServerInitialize(const FTransform& Transform, float StateOfCharge)
{
	check(HasAuthority());
	SetActorTransform(Transform);
	RefreshParams();
	Sim = FPVSim::State();
	FPVSim::ChargeBattery(Params, Sim);
	// The battery keeps its charge while the drone sits in the inventory.
	Sim.StateOfCharge = 0.0f;
	Sim.MahUsed = Params.CapacityMah * Params.CapacityMultiplier;
	FPVSim::AddCharge(Params, Sim, FMath::Clamp(StateOfCharge, 0.0f, 1.0f));
	Sim.Voltage = FPVSim::OpenCircuitVoltage(Params, Sim.StateOfCharge);
	Sim.Pos = WorldToSim(Transform.GetLocation());
	Sim.Rot = WorldToSim(Transform.GetRotation());
	ReplicatedState = MakeNetState();
	SavedStateOfCharge = Sim.StateOfCharge;
	SavedMahUsed = Sim.MahUsed;
	ChargeCheckTimer = 0.0f; // check for power right away
}

void AFPVDroneActor::ServerSetPiloted(bool bNewPiloted)
{
	check(HasAuthority());
	bPiloted = bNewPiloted;
	if (bPiloted)
	{
		bServerSimActive = false;
	}
	else
	{
		// Take over the physics from the last known pilot state; the drone drops disarmed.
		ApplyNetStateToSim(ReplicatedState);
		bServerSimActive = true;
		RestTimer = 0.0f;
	}
	ForceNetUpdate();
	OnRep_Piloted();
}

void AFPVDroneActor::OnRep_Piloted()
{
	// Actual switching happens in Tick so it also works when the owner replicates late.
}

void AFPVDroneActor::BeginLocalPiloting()
{
	APlayerController* PC = GetOwningPlayerController();
	if (PC == nullptr)
	{
		return;
	}
	bLocallyPiloting = true;

	PilotPawn = PC->GetPawn();
	if (APawn* Pawn = PilotPawn.Get())
	{
		Pawn->DisableInput(PC);
		HomeLocation = Pawn->GetActorLocation();
		// Hide the pilot while flying, otherwise the drone camera sees the (first-person) body as a
		// floating head standing at the launch spot.
		Pawn->SetActorHiddenInGame(true);
	}
	else
	{
		HomeLocation = GetActorLocation();
	}

	// Hide the pilot's HUD (hotbar, health, tool menu, crosshair) so only the goggles OSD shows.
	// SetHUDVisibility only affects the canvas HUD, so collapse the UMG game UI widget directly.
	if (AFGHUD* HUD = PC->GetHUD<AFGHUD>())
	{
		HUD->SetHiddenHUDMode(true);
		if (UFGGameUI* GameUI = HUD->GetGameUI())
		{
			GameUI->SetVisibility(ESlateVisibility::Collapsed);
		}
		UE_LOG(LogFPVDrone, Log, TEXT("FPV: HUD hidden (GameUI=%s)"), HUD->GetGameUI() ? TEXT("ok") : TEXT("null"));
	}
	else
	{
		UE_LOG(LogFPVDrone, Warning, TEXT("FPV: no AFGHUD found to hide"));
	}

	RefreshParams();
	ApplyNetStateToSim(ReplicatedState);
	Sim.Pos = WorldToSim(GetActorLocation());
	Sim.Rot = WorldToSim(GetActorQuat());
	StepAccumulator = 0.0;
	Buffer.Reset();

	bArmed = false;
	bArmSwitchSeenOff = false;
	FlightTime = 0.0f;
	FailsafeTimer = 0.0f;
	bFailsafe = false;
	LinkQuality = 1.0f;

	Camera->SetActive(true);
	ApplyCameraSettings();
	PC->SetViewTargetWithBlend(this, 0.2f);

	if (GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		OSD = SNew(SFPVOSD)
			.Drone(this)
			.Vignette(TextureVignette)
			.StaticNoise(TextureStatic);
		GEngine->GameViewport->AddViewportWidgetContent(OSD.ToSharedRef(), 10);
	}
	UE_LOG(LogFPVDrone, Log, TEXT("Goggles on"));
}

void AFPVDroneActor::EndLocalPiloting()
{
	if (!bLocallyPiloting)
	{
		return;
	}
	bLocallyPiloting = false;
	bArmed = false;

	if (OSD.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(OSD.ToSharedRef());
	}
	OSD.Reset();

	APlayerController* PC = GetOwningPlayerController();
	APawn* Pawn = PilotPawn.Get();
	if (PC != nullptr)
	{
		if (Pawn == nullptr)
		{
			Pawn = PC->GetPawn();
		}
		if (Pawn != nullptr)
		{
			Pawn->EnableInput(PC);
			Pawn->SetActorHiddenInGame(false);
			PC->SetViewTargetWithBlend(Pawn, 0.2f);
		}
		if (AFGHUD* HUD = PC->GetHUD<AFGHUD>())
		{
			HUD->SetHiddenHUDMode(false);
			if (UFGGameUI* GameUI = HUD->GetGameUI())
			{
				GameUI->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
		}
	}
	PilotPawn.Reset();
	UE_LOG(LogFPVDrone, Log, TEXT("Goggles off"));
}

// --- Tick -------------------------------------------------------------------------

void AFPVDroneActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bShouldPilot = IsLocallyPiloted();
	if (bShouldPilot && !bLocallyPiloting)
	{
		BeginLocalPiloting();
	}
	else if (!bShouldPilot && bLocallyPiloting)
	{
		EndLocalPiloting();
	}

	if (HasAuthority())
	{
		TickServerCharging(DeltaSeconds);
	}

	if (bLocallyPiloting)
	{
		TickLocalPilot(DeltaSeconds);
	}
	else if (HasAuthority() && !bPiloted)
	{
		if (bServerSimActive)
		{
			TickServerSim(DeltaSeconds);
		}
	}
	else
	{
		TickInterpolation(DeltaSeconds);
	}

	UpdateVisuals(DeltaSeconds);
}

void AFPVDroneActor::RefreshParams()
{
	if (bParamsDirty)
	{
		Params = FFPVSettings::ToSimParams(FFPVSettings::Get());
		bParamsDirty = false;
	}
}

void AFPVDroneActor::TickLocalPilot(float DeltaSeconds)
{
	APlayerController* PC = GetOwningPlayerController();
	const FFPVConfig& Config = FFPVSettings::Get();
	const FFPVPilotInput Input = FFPVInputSystem::Get().Sample(PC);
	InputSourceName = Input.SourceName;

	UpdateSignal(DeltaSeconds);

	const bool bLinkLost = !Input.bValid || LinkQuality <= 0.02f;
	FailsafeTimer = bLinkLost ? FailsafeTimer + DeltaSeconds : 0.0f;
	bFailsafe = Config.Gameplay.bFailsafe && FailsafeTimer > 1.0f;

	UpdateArming(Input);
	LastThrottle = Input.Throttle;

	FPVSim::Input SimInput;
	SimInput.Roll = Input.Roll;
	SimInput.Pitch = Input.Pitch;
	SimInput.Yaw = Input.Yaw;
	SimInput.Throttle = Input.Throttle;
	SimInput.Armed = bArmed;
	SimInput.Mode = static_cast<FPVSim::FlightMode>(Mode);
	SimInput.Mode3D = bMode3D;
	SimInput.Turtle = bTurtle;

	StepSimulation(SimInput, DeltaSeconds);
	if (bChargerInRange)
	{
		// The pilot owns the battery state while flying; the server checks the rate.
		FPVSim::AddCharge(Params, Sim, DeltaSeconds / FPVPowerGrid::SecondsToFull);
	}
	MoveWithCollision(DeltaSeconds, true);
	if (bInContact && (!bArmed || (!bTurtle && FMath::Abs(Sim.ThrottleOut) < 0.1f)))
	{
		SettleOnGround(DeltaSeconds);
	}
	if (bArmed)
	{
		FlightTime += DeltaSeconds;
	}

	SendTimer += DeltaSeconds;
	if (SendTimer >= SendInterval)
	{
		SendTimer = 0.0f;
		const FFPVNetState State = MakeNetState();
		if (HasAuthority())
		{
			ReplicatedState = State;
		}
		else
		{
			Server_SubmitState(State);
		}
	}

	ApplyCameraSettings();
}

void AFPVDroneActor::UpdateArming(const FFPVPilotInput& Input)
{
	const FFPVConfig& Config = FFPVSettings::Get();

	const bool bNew3D = Input.b3D || Config.Gameplay.bAlways3D;
	if (bNew3D != bMode3D || Input.Mode != Mode || Input.bTurtle != bTurtle)
	{
		PlaySound2D(SoundModeSwitch);
	}
	// Switching 3D in the air would flip the throttle meaning, Betaflight only allows it disarmed.
	if (!bArmed)
	{
		bMode3D = bNew3D;
	}
	Mode = Input.Mode;
	bTurtle = Input.bTurtle;

	// An empty pack browns out the flight controller: power is lost, the motors cut and the craft
	// drops, exactly like a real LiPo hitting 0%. Re-arming needs a recharge (near a power pole).
	const bool bBatteryEmpty = Sim.StateOfCharge <= 0.0f;

	if (!Input.bArmSwitch || bFailsafe || bBatteryEmpty)
	{
		if (bArmed)
		{
			bArmed = false;
			PlaySound2D((bFailsafe || bBatteryEmpty) ? SoundWarn : SoundDisarm);
		}
		if (!Input.bArmSwitch)
		{
			bArmSwitchSeenOff = true;
			ArmBlockReason = FText::GetEmpty();
		}
		if (bFailsafe)
		{
			ArmBlockReason = LOCTEXT("BlockFailsafe", "FAILSAFE");
		}
		else if (bBatteryEmpty)
		{
			ArmBlockReason = LOCTEXT("BlockEmpty", "BATTERY EMPTY");
		}
		return;
	}

	if (bArmed)
	{
		return;
	}

	if (!bArmSwitchSeenOff)
	{
		ArmBlockReason = LOCTEXT("BlockSwitch", "ARM SWITCH");
		return;
	}
	const bool bThrottleLow = bMode3D ? FMath::Abs(Input.Throttle - 0.5f) < 0.05f : Input.Throttle < 0.05f;
	if (Config.Radio.bArmRequiresLowThrottle && !bThrottleLow)
	{
		ArmBlockReason = LOCTEXT("BlockThrottle", "THROTTLE");
		return;
	}
	const FPVSim::Vec3 Up = Sim.Rot.Rotate({ 0.0f, 0.0f, 1.0f });
	if (!bTurtle && !bMode3D && Up.Z < FMath::Cos(FMath::DegreesToRadians(25.0f)))
	{
		ArmBlockReason = LOCTEXT("BlockAngle", "ANGLE - USE TURTLE");
		return;
	}

	bArmed = true;
	ArmBlockReason = FText::GetEmpty();
	FPVSim::ResetController(Sim);
	PlaySound2D(SoundArm);
}

void AFPVDroneActor::UpdateSignal(float DeltaSeconds)
{
	const FFPVGameplaySettings& Gameplay = FFPVSettings::Get().Gameplay;
	APawn* Pawn = PilotPawn.Get();
	if (Pawn == nullptr)
	{
		LinkQuality = 1.0f;
		return;
	}

	const FVector PilotEye = Pawn->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
	const float DistanceM = static_cast<float>(FVector::Dist(PilotEye, GetActorLocation()) / 100.0);

	SignalTraceTimer -= DeltaSeconds;
	if (SignalTraceTimer <= 0.0f && Gameplay.bObstaclesReduceSignal)
	{
		SignalTraceTimer = 0.25f;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(FPVDroneSignal), false);
		Query.AddIgnoredActor(this);
		Query.AddIgnoredActor(Pawn);
		FHitResult Hit;
		bLineOfSight = !GetWorld()->LineTraceSingleByChannel(Hit, PilotEye, GetActorLocation(), ECC_Visibility, Query);
	}

	float Quality = 1.0f;
	if (Gameplay.MaxRangeMeters > 0.0f)
	{
		Quality = FMath::Clamp(1.0f - FMath::Pow(DistanceM / Gameplay.MaxRangeMeters, 3.0f), 0.0f, 1.0f);
	}
	if (Gameplay.bObstaclesReduceSignal && !bLineOfSight)
	{
		// Behind walls the link degrades quickly with distance (2.4 GHz video, ELRS control).
		Quality *= FMath::GetMappedRangeValueClamped(FVector2f(20.0f, 400.0f), FVector2f(0.9f, 0.1f), DistanceM);
	}
	LinkQuality = FMath::FInterpTo(LinkQuality, Quality, DeltaSeconds, 4.0f);
}

void AFPVDroneActor::ApplyCameraSettings()
{
	const FFPVCameraSettings& Settings = FFPVSettings::Get().Camera;
	Camera->SetFieldOfView(Settings.FieldOfView);
	CameraPivot->SetRelativeRotation(FRotator(Settings.UptiltDeg, 0.0f, 0.0f));
	if (Settings.Mode == EFPVCameraMode::Chase)
	{
		// Chase camera behind/above the drone, but trace against the world so it does not clip
		// through the ground or walls.
		const float Dist = Settings.ChaseDistanceCm;
		const FVector PivotWorld = CameraPivot->GetComponentLocation();
		const FVector Behind = GetActorQuat().RotateVector(FVector(-Dist, 0.0f, Dist * 0.3f));
		FVector CamWorld = PivotWorld + Behind;
		if (UWorld* World = GetWorld())
		{
			FHitResult Hit;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(FPVChaseCam), false, this);
			if (World->LineTraceSingleByChannel(Hit, PivotWorld, CamWorld, ECC_Camera, Query))
			{
				CamWorld = Hit.Location - Behind.GetSafeNormal() * 10.0f;
			}
		}
		Camera->SetWorldLocation(CamWorld);
		Camera->SetWorldRotation(FRotationMatrix::MakeFromX(PivotWorld - CamWorld).Rotator());
		CameraBody->SetOwnerNoSee(false);
	}
	else
	{
		Camera->SetRelativeLocation(FVector(2.0f, 0.0f, 0.0f));
		Camera->SetRelativeRotation(FRotator::ZeroRotator);
		CameraBody->SetOwnerNoSee(true);
	}
	AppliedCameraMode = Settings.Mode;
}

// --- Physics ----------------------------------------------------------------------

void AFPVDroneActor::StepSimulation(const FPVSim::Input& Input, float DeltaSeconds)
{
	RefreshParams();
	StepAccumulator += FMath::Min(DeltaSeconds, 0.1f);
	int32 Steps = 0;
	while (StepAccumulator >= FPVSim::StepDt && Steps < 150)
	{
		FPVSim::Step(Params, Input, Sim);
		StepAccumulator -= FPVSim::StepDt;
		++Steps;
	}
	if (Steps >= 150)
	{
		StepAccumulator = 0.0;
	}
}

void AFPVDroneActor::MoveWithCollision(float DeltaSeconds, bool bAllowCrash)
{
	const FVector Target = SimToWorld(Sim.Pos);
	const FQuat Rotation = SimToWorld(Sim.Rot);

	FHitResult Hit;
	SetActorLocationAndRotation(Target, Rotation, true, &Hit, ETeleportType::None);

	bInContact = Hit.bBlockingHit;
	if (!Hit.bBlockingHit)
	{
		ContactTimer = 0.0f;
		return;
	}
	ContactTimer += DeltaSeconds;

	if (Hit.bStartPenetrating)
	{
		AddActorWorldOffset(Hit.Normal * (Hit.PenetrationDepth + 0.5f), false);
	}
	Sim.Pos = WorldToSim(GetActorLocation());

	const FPVSim::Vec3 Normal = DirToSim(Hit.ImpactNormal.IsNearlyZero() ? Hit.Normal : Hit.ImpactNormal).Normalized();
	const float NormalSpeed = Sim.Vel.Dot(Normal);
	if (NormalSpeed >= 0.0f)
	{
		return;
	}

	const float Impact = -NormalSpeed;
	const FFPVCraft& Craft = FFPVSettings::Get().Craft;
	const float Restitution = Impact > 1.0f ? Craft.BounceRestitution : 0.0f;
	Sim.Vel -= Normal * ((1.0f + Restitution) * NormalSpeed);
	const FPVSim::Vec3 Tangent = Sim.Vel - Normal * Sim.Vel.Dot(Normal);
	Sim.Vel = Normal * Sim.Vel.Dot(Normal) + Tangent * 0.8f;

	if (Impact > 2.0f)
	{
		// Prop strikes throw the craft around.
		Sim.Omega *= 0.5f;
		Sim.Omega += FPVSim::Vec3(Sim.Random.NextSigned(), Sim.Random.NextSigned(), Sim.Random.NextSigned()) * (Impact * 1.5f);
	}

	if (bAllowCrash && bArmed && Impact > Craft.CrashDisarmSpeed)
	{
		bArmed = false;
		bArmSwitchSeenOff = false;
		ArmBlockReason = LOCTEXT("Crash", "CRASH - ARM SWITCH OFF");
		PlaySound2D(SoundCrash);
	}
}

void AFPVDroneActor::SettleOnGround(float DeltaSeconds)
{
	// A disarmed quad lies flat, either upright or on its back (then it needs turtle mode).
	const FPVSim::Vec3 Up = Sim.Rot.Rotate({ 0.0f, 0.0f, 1.0f });
	const FPVSim::Vec3 TargetUp(0.0f, 0.0f, Up.Z >= 0.0f ? 1.0f : -1.0f);
	const FPVSim::Vec3 Axis = Up.Cross(TargetUp);
	const float Angle = FMath::Acos(FMath::Clamp(Up.Dot(TargetUp), -1.0f, 1.0f));
	if (Axis.Size() > 1e-4f && Angle > 1e-3f)
	{
		const float Step = Angle * FMath::Min(1.0f, 8.0f * DeltaSeconds);
		Sim.Rot = FPVSim::Quat::FromAxisAngle(Axis, Step) * Sim.Rot;
		Sim.Rot.Normalize();
	}
	Sim.Omega *= FMath::Exp(-10.0f * DeltaSeconds);
	if (Sim.Vel.Size() < 0.3f)
	{
		Sim.Vel *= FMath::Exp(-10.0f * DeltaSeconds);
	}
}

void AFPVDroneActor::TickServerCharging(float DeltaSeconds)
{
	ChargeCheckTimer -= DeltaSeconds;
	if (ChargeCheckTimer <= 0.0f)
	{
		ChargeCheckTimer = 1.0f;
		const bool bInRange = FPVPowerGrid::IsNearPoweredGrid(GetWorld(), GetActorLocation());
		if (bInRange != bChargerInRange)
		{
			bChargerInRange = bInRange;
			ForceNetUpdate();
		}
	}
	ChargerGraceTimer = bChargerInRange ? 2.0f : FMath::Max(0.0f, ChargerGraceTimer - DeltaSeconds);

	if (bPiloted || !bChargerInRange || ReplicatedState.StateOfCharge >= 1.0f)
	{
		return;
	}
	RefreshParams();
	Sim.StateOfCharge = ReplicatedState.StateOfCharge;
	Sim.MahUsed = ReplicatedState.MahUsed;
	FPVSim::AddCharge(Params, Sim, DeltaSeconds / FPVPowerGrid::SecondsToFull);
	ReplicatedState.StateOfCharge = Sim.StateOfCharge;
	ReplicatedState.MahUsed = Sim.MahUsed;

	ChargePublishTimer += DeltaSeconds;
	if (ChargePublishTimer >= 1.0f || Sim.StateOfCharge >= 1.0f)
	{
		ChargePublishTimer = 0.0f;
		ForceNetUpdate();
	}
}

void AFPVDroneActor::TickServerSim(float DeltaSeconds)
{
	FPVSim::Input Input; // disarmed
	StepSimulation(Input, DeltaSeconds);
	MoveWithCollision(DeltaSeconds, false);
	if (bInContact)
	{
		SettleOnGround(DeltaSeconds);
	}

	const bool bResting = bInContact && Sim.Vel.Size() < 0.05f && Sim.Omega.Size() < 0.1f;
	RestTimer = bResting ? RestTimer + DeltaSeconds : 0.0f;

	SendTimer += DeltaSeconds;
	if (SendTimer >= ServerSendInterval || RestTimer > 1.0f)
	{
		SendTimer = 0.0f;
		ReplicatedState = MakeNetState();
	}
	if (RestTimer > 1.0f)
	{
		bServerSimActive = false;
	}
}

// --- Networking ---------------------------------------------------------------------

FFPVNetState AFPVDroneActor::MakeNetState() const
{
	FFPVNetState State;
	State.Location = GetActorLocation();
	State.Rotation = GetActorQuat();
	State.Velocity = DirToWorld(Sim.Vel) * 100.0;
	State.AngularVelocity = AxialToWorld(Sim.Omega);
	State.Time = Sim.Time;

	float Motor = 0.0f;
	for (const float Output : Sim.MotorOutput)
	{
		Motor += FMath::Abs(Output) * 0.25f;
	}
	State.MotorLevel = static_cast<uint8>(FMath::Clamp(Motor * 255.0f, 0.0f, 255.0f));

	EFPVNetFlags Flags = EFPVNetFlags::None;
	if (bArmed) { Flags |= EFPVNetFlags::Armed; }
	if (bMode3D) { Flags |= EFPVNetFlags::Mode3D; }
	if (bTurtle) { Flags |= EFPVNetFlags::Turtle; }
	if (bFailsafe) { Flags |= EFPVNetFlags::Failsafe; }
	State.Flags = static_cast<uint8>(Flags);
	State.Mode = static_cast<uint8>(Mode);
	State.StateOfCharge = Sim.StateOfCharge;
	State.MahUsed = Sim.MahUsed;
	return State;
}

void AFPVDroneActor::ApplyNetStateToSim(const FFPVNetState& State)
{
	Sim.Pos = WorldToSim(State.Location);
	Sim.Rot = WorldToSim(State.Rotation);
	Sim.Vel = DirToSim(State.Velocity) * 0.01f;
	Sim.Omega = AxialToSim(State.AngularVelocity);
	Sim.Time = State.Time;
	Sim.StateOfCharge = State.StateOfCharge;
	Sim.MahUsed = State.MahUsed;
	Sim.Voltage = FPVSim::OpenCircuitVoltage(Params, Sim.StateOfCharge);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Sim.MotorCommand[Index] = 0.0f;
		Sim.MotorOutput[Index] = 0.0f;
	}
}

void AFPVDroneActor::Server_SubmitState_Implementation(const FFPVNetState& InState)
{
	if (!bPiloted)
	{
		return;
	}
	FFPVNetState State = InState;
	const FFPVNetState& Previous = ReplicatedState;

	if (State.Time <= Previous.Time && Previous.Time - State.Time < 1.0f)
	{
		return; // out of order
	}

	// Sanity checks: no teleporting, no recharging away from power.
	const float Elapsed = FMath::Clamp(State.Time - Previous.Time, 0.0f, 2.0f);
	const float Allowed = MaxServerSpeedCm * Elapsed + 500.0f;
	const FVector Delta = FVector(State.Location) - FVector(Previous.Location);
	if (Delta.Size() > Allowed)
	{
		UE_LOG(LogFPVDrone, Warning, TEXT("Rejected drone jump of %.0f cm"), Delta.Size());
		State.Location = FVector(Previous.Location) + Delta.GetSafeNormal() * Allowed;
	}
	// Charging is only allowed near power, and no faster than the charger works.
	const float MaxCharge = ChargerGraceTimer > 0.0f ? Elapsed / FPVPowerGrid::SecondsToFull * 1.25f + 0.01f : 0.001f;
	State.StateOfCharge = FMath::Min(State.StateOfCharge, Previous.StateOfCharge + MaxCharge);

	ReplicatedState = State;
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorLocationAndRotation(State.Location, State.Rotation);
	}
	else
	{
		OnRep_State(); // listen server: interpolate like a client
	}
}

void AFPVDroneActor::OnRep_State()
{
	if (bLocallyPiloting)
	{
		return;
	}
	const FFPVNetState& State = ReplicatedState;
	if (Buffer.Num() > 0)
	{
		const float LastTime = Buffer.Last().Time;
		if (State.Time < LastTime - 0.5f)
		{
			Buffer.Reset(); // new timeline (pilot changed or server took over)
		}
		else if (State.Time <= LastTime)
		{
			return;
		}
	}
	if (Buffer.Num() == 0)
	{
		PlaybackTime = State.Time - InterpolationDelay;
		SetActorLocationAndRotation(State.Location, State.Rotation);
	}
	Buffer.Add(State);
	if (Buffer.Num() > 32)
	{
		Buffer.RemoveAt(0, Buffer.Num() - 32);
	}
}

void AFPVDroneActor::TickInterpolation(float DeltaSeconds)
{
	if (Buffer.Num() == 0)
	{
		return;
	}
	const FFPVNetState& Newest = Buffer.Last();
	const double Target = Newest.Time - InterpolationDelay;
	PlaybackTime += DeltaSeconds;
	if (FMath::Abs(PlaybackTime - Target) > 0.3)
	{
		PlaybackTime = Target;
	}
	else
	{
		PlaybackTime += (Target - PlaybackTime) * FMath::Min(1.0, DeltaSeconds * 2.0);
	}

	FVector Location;
	FQuat Rotation;
	if (PlaybackTime >= Newest.Time)
	{
		const float Ahead = static_cast<float>(FMath::Min(PlaybackTime - Newest.Time, 0.25));
		Location = FVector(Newest.Location) + FVector(Newest.Velocity) * Ahead;
		Rotation = Newest.Rotation;
	}
	else if (PlaybackTime <= Buffer[0].Time)
	{
		Location = Buffer[0].Location;
		Rotation = Buffer[0].Rotation;
	}
	else
	{
		int32 Index = Buffer.Num() - 2;
		while (Index > 0 && Buffer[Index].Time > PlaybackTime)
		{
			--Index;
		}
		const FFPVNetState& A = Buffer[Index];
		const FFPVNetState& B = Buffer[Index + 1];
		const float Span = FMath::Max(B.Time - A.Time, 1e-4f);
		const float Alpha = FMath::Clamp(static_cast<float>((PlaybackTime - A.Time) / Span), 0.0f, 1.0f);
		Location = FMath::CubicInterp(FVector(A.Location), FVector(A.Velocity) * Span, FVector(B.Location), FVector(B.Velocity) * Span, Alpha);
		Rotation = FQuat::Slerp(A.Rotation, B.Rotation, Alpha);
	}
	SetActorLocationAndRotation(Location, Rotation);

	while (Buffer.Num() > 2 && Buffer[1].Time < PlaybackTime - 1.0)
	{
		Buffer.RemoveAt(0);
	}
}

// --- Visuals / audio ----------------------------------------------------------------

void AFPVDroneActor::UpdateVisuals(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const float Average = bLocallyPiloting || HasAuthority()
		? (FMath::Abs(Sim.MotorOutput[0]) + FMath::Abs(Sim.MotorOutput[1]) + FMath::Abs(Sim.MotorOutput[2]) + FMath::Abs(Sim.MotorOutput[3])) * 0.25f
		: ReplicatedState.MotorLevel / 255.0f;

	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float Speed = bLocallyPiloting ? FMath::Abs(Sim.MotorOutput[Index]) : Average;
		const float Direction = (bLocallyPiloting && Sim.MotorOutput[Index] < 0.0f) ? -1.0f : 1.0f;
		PropAngle[Index] = FMath::Fmod(PropAngle[Index] + MotorVisualSign[Index] * Direction * Speed * VisualPropRevPerSec * 360.0f * DeltaSeconds, 360.0f);
		Props[Index]->SetRelativeRotation(FRotator(0.0f, PropAngle[Index], 0.0f));

		const bool bBlur = PropBlurs[Index]->GetStaticMesh() != nullptr && Speed > BlurThreshold;
		PropBlurs[Index]->SetVisibility(bBlur);
		Props[Index]->SetVisibility(!bBlur, false);
	}

	if (MotorAudio->Sound != nullptr)
	{
		if (Average > 0.01f)
		{
			if (!MotorAudio->IsPlaying())
			{
				MotorAudio->Play();
			}
			MotorAudio->SetPitchMultiplier(0.6f + 1.4f * Average);
			MotorAudio->SetVolumeMultiplier(FMath::Clamp(0.25f + Average * 1.5f, 0.0f, 1.0f));
		}
		else if (MotorAudio->IsPlaying())
		{
			MotorAudio->Stop();
		}
	}

	if (PropwashAudio->Sound != nullptr && bLocallyPiloting)
	{
		const float Descent = -Sim.Rot.Unrotate(Sim.Vel).Z;
		const float Wash = FMath::Clamp((Descent - 1.5f) * 0.25f, 0.0f, 1.0f) * FMath::Clamp(FMath::Abs(Sim.ThrottleOut) * 2.0f, 0.0f, 1.0f);
		if (Wash > 0.05f && !PropwashAudio->IsPlaying())
		{
			PropwashAudio->Play();
		}
		else if (Wash <= 0.05f && PropwashAudio->IsPlaying())
		{
			PropwashAudio->Stop();
		}
		PropwashAudio->SetVolumeMultiplier(Wash);
	}
}

void AFPVDroneActor::PlaySound2D(USoundBase* Sound) const
{
	if (Sound != nullptr && bLocallyPiloting)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
}

// --- Telemetry ------------------------------------------------------------------------

FFPVTelemetry AFPVDroneActor::GetTelemetry() const
{
	FFPVTelemetry T;
	T.Voltage = Sim.Voltage;
	T.CellVoltage = Sim.Voltage / FMath::Max(1, Params.Cells);
	T.Current = Sim.Current;
	T.MahUsed = Sim.MahUsed;
	T.StateOfCharge = Sim.StateOfCharge;
	T.FlightTime = FlightTime;
	T.SpeedKmh = Sim.Vel.Size() * 3.6f;
	T.AltitudeM = static_cast<float>((GetActorLocation().Z - HomeLocation.Z) / 100.0);

	const FVector ToHome = HomeLocation - GetActorLocation();
	T.HomeDistanceM = static_cast<float>(ToHome.Size2D() / 100.0);
	T.HomeDirectionDeg = FRotator::NormalizeAxis(static_cast<float>(ToHome.Rotation().Yaw - GetActorRotation().Yaw));

	T.ThrottlePercent = LastThrottle * 100.0f;
	float Roll, Pitch;
	Sim.Rot.ToRollPitch(Roll, Pitch);
	T.RollDeg = FMath::RadiansToDegrees(Roll);
	T.PitchDeg = FMath::RadiansToDegrees(Pitch);
	T.LinkQuality = FMath::RoundToInt(LinkQuality * 100.0f);
	T.Mode = Mode;
	T.bArmed = bArmed;
	T.b3D = bMode3D;
	T.bTurtle = bTurtle;
	T.bAirmode = Params.Airmode;
	T.bFailsafe = bFailsafe;
	T.bLowBattery = T.CellVoltage < 3.5f;
	T.bCharging = bChargerInRange && Sim.StateOfCharge < 1.0f;
	T.ArmBlockReason = ArmBlockReason;
	T.InputSource = InputSourceName;
	return T;
}

// --- Use (pick up with E) -------------------------------------------------------------

bool AFPVDroneActor::IsUseable_Implementation() const
{
	// Not while someone flies it.
	return !bPiloted;
}

FText AFPVDroneActor::GetLookAtDecription_Implementation(AFGCharacterPlayer* ByCharacter, const FUseState& State) const
{
	return FText::Format(LOCTEXT("LookAtDrone", "Pick up FPV Drone ({0}%)"), FText::AsNumber(FMath::RoundToInt(ReplicatedState.StateOfCharge * 100.0f)));
}

void AFPVDroneActor::OnUse_Implementation(AFGCharacterPlayer* ByCharacter, const FUseState& State)
{
	if (!HasAuthority() || ByCharacter == nullptr || bPiloted)
	{
		return;
	}
	if (AFPVPilotLink* Link = AFPVPilotLink::FindFor(ByCharacter->GetController<APlayerController>()))
	{
		Link->PickUp(this);
	}
}

// --- Save game ---------------------------------------------------------------------------

void AFPVDroneActor::PreSaveGame_Implementation(int32 SaveVersion, int32 GameVersion)
{
	SavedStateOfCharge = ReplicatedState.StateOfCharge;
	SavedMahUsed = ReplicatedState.MahUsed;
}

void AFPVDroneActor::PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion)
{
	bPiloted = false;
	bServerSimActive = false;
	ReplicatedState.Location = GetActorLocation();
	ReplicatedState.Rotation = GetActorQuat();
	ReplicatedState.StateOfCharge = SavedStateOfCharge;
	ReplicatedState.MahUsed = SavedMahUsed;
	ReplicatedState.Time = 0.0f;
	ApplyNetStateToSim(ReplicatedState);
}

#undef LOCTEXT_NAMESPACE
