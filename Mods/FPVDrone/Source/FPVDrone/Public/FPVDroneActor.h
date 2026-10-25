// The FPV drone in the world.
//
// Network model: the pilot's client runs the flight simulation locally at 1 kHz (no input lag)
// and sends its state to the server at 30 Hz. The server sanity-checks and replicates it, other
// clients interpolate. Without a pilot the server simulates the (disarmed) drone until it rests.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/NetSerialization.h"
#include "FGSaveInterface.h"
#include "FGUseableInterface.h"
#include "FPVTypes.h"
#include "Sim/FPVFlightSim.h"
#include "FPVDroneActor.generated.h"

class APlayerController;
class UAudioComponent;
class UCameraComponent;
class USphereComponent;
class UStaticMeshComponent;
class USoundBase;
class UTexture2D;
class SFPVOSD;

enum class EFPVNetFlags : uint8
{
	None = 0,
	Armed = 1 << 0,
	Mode3D = 1 << 1,
	Turtle = 1 << 2,
	Failsafe = 1 << 3,
};
ENUM_CLASS_FLAGS(EFPVNetFlags);

USTRUCT()
struct FFPVNetState
{
	GENERATED_BODY()

	UPROPERTY() FVector_NetQuantize10 Location;
	UPROPERTY() FQuat Rotation = FQuat::Identity;
	UPROPERTY() FVector_NetQuantize10 Velocity;			// cm/s
	UPROPERTY() FVector_NetQuantize100 AngularVelocity;	// rad/s, UE frame
	UPROPERTY() float Time = 0.0f;						// sender clock
	UPROPERTY() uint8 MotorLevel = 0;					// average motor output 0..255
	UPROPERTY() uint8 Flags = 0;						// EFPVNetFlags
	UPROPERTY() uint8 Mode = 0;							// EFPVFlightMode
	UPROPERTY() float StateOfCharge = 1.0f;
	UPROPERTY() float MahUsed = 0.0f;
};

// Values for the OSD.
struct FFPVTelemetry
{
	float Voltage = 0.0f;
	float CellVoltage = 0.0f;
	float Current = 0.0f;
	float MahUsed = 0.0f;
	float StateOfCharge = 1.0f;
	float FlightTime = 0.0f;
	float SpeedKmh = 0.0f;
	float AltitudeM = 0.0f;
	float HomeDistanceM = 0.0f;
	float HomeDirectionDeg = 0.0f;	// relative to the nose, + = right
	float ThrottlePercent = 0.0f;
	float RollDeg = 0.0f;
	float PitchDeg = 0.0f;
	int32 LinkQuality = 100;
	EFPVFlightMode Mode = EFPVFlightMode::Acro;
	bool bArmed = false;
	bool b3D = false;
	bool bTurtle = false;
	bool bAirmode = true;
	bool bFailsafe = false;
	bool bLowBattery = false;
	bool bCharging = false;
	FText ArmBlockReason;
	FString InputSource;
};

UCLASS()
class FPVDRONE_API AFPVDroneActor : public AActor, public IFGSaveInterface, public IFGUseableInterface
{
	GENERATED_BODY()

public:
	AFPVDroneActor();

	// AActor
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IFGSaveInterface
	virtual void PreSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override;
	virtual void PostSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void PreLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override;
	virtual void GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects) override {}
	virtual bool NeedTransform_Implementation() override { return true; }
	virtual bool ShouldSave_Implementation() const override { return true; }

	// IFGUseableInterface: look at the drone and press E (the game's "use" key) to pick it up.
	virtual void UpdateUseState_Implementation(class AFGCharacterPlayer* ByCharacter, const FVector& AtLocation, class UPrimitiveComponent* ComponentHit, FUseState& OutUseState) override {}
	virtual void OnUse_Implementation(class AFGCharacterPlayer* ByCharacter, const FUseState& State) override;
	virtual void OnUseStop_Implementation(class AFGCharacterPlayer* ByCharacter, const FUseState& State) override {}
	virtual bool IsUseable_Implementation() const override;
	virtual void StartIsLookedAt_Implementation(class AFGCharacterPlayer* ByCharacter, const FUseState& State) override {}
	virtual void StopIsLookedAt_Implementation(class AFGCharacterPlayer* ByCharacter, const FUseState& State) override {}
	virtual FText GetLookAtDecription_Implementation(class AFGCharacterPlayer* ByCharacter, const FUseState& State) const override;
	virtual void RegisterInteractingPlayer_Implementation(class AFGCharacterPlayer* Player) override {}
	virtual void UnregisterInteractingPlayer_Implementation(class AFGCharacterPlayer* Player) override {}

	// --- Server API (called by AFPVPilotLink) ----------------------------------
	void ServerSetPiloted(bool bNewPiloted);
	void ServerInitialize(const FTransform& Transform, float StateOfCharge = 1.0f);
	bool IsPiloted() const { return bPiloted; }
	float GetStateOfCharge() const { return ReplicatedState.StateOfCharge; }

	// True on the machine of the player currently flying this drone.
	bool IsLocallyPiloted() const;

	APlayerController* GetOwningPlayerController() const;

	FFPVTelemetry GetTelemetry() const;

	UFUNCTION(Server, Unreliable)
	void Server_SubmitState(const FFPVNetState& State);

protected:
	UFUNCTION()
	void OnRep_State();

	UFUNCTION()
	void OnRep_Piloted();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Frame;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> CameraPivot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> CameraBody;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> Props;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> PropBlurs;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> MotorAudio;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAudioComponent> PropwashAudio;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FFPVNetState ReplicatedState;

	UPROPERTY(ReplicatedUsing = OnRep_Piloted)
	bool bPiloted = false;

	// Set by the server while a powered pole or power line is in range (see FPVPowerGrid).
	UPROPERTY(Replicated)
	bool bChargerInRange = false;

	UPROPERTY(SaveGame)
	float SavedStateOfCharge = 1.0f;

	UPROPERTY(SaveGame)
	float SavedMahUsed = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> SoundArm;
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> SoundDisarm;
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> SoundCrash;
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> SoundModeSwitch;
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> SoundWarn;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TextureVignette;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TextureStatic;

private:
	void SetupVisuals();
	void LoadAssets();

	// Local pilot
	void BeginLocalPiloting();
	void EndLocalPiloting();
	void TickLocalPilot(float DeltaSeconds);
	void UpdateArming(const FFPVPilotInput& Input);
	void UpdateSignal(float DeltaSeconds);
	void ApplyCameraSettings();

	// Physics shared by pilot and server
	void StepSimulation(const FPVSim::Input& Input, float DeltaSeconds);
	void MoveWithCollision(float DeltaSeconds, bool bAllowCrash);
	void SettleOnGround(float DeltaSeconds);

	// Server without pilot
	void TickServerSim(float DeltaSeconds);

	// Server: look for power nearby, charge the battery while nobody flies the drone.
	void TickServerCharging(float DeltaSeconds);

	// Remote view
	void TickInterpolation(float DeltaSeconds);

	void UpdateVisuals(float DeltaSeconds);
	void PlaySound2D(USoundBase* Sound) const;

	FFPVNetState MakeNetState() const;
	void ApplyNetStateToSim(const FFPVNetState& State);
	void RefreshParams();

	// Conversions between the simulation frame (FLU, metres) and Unreal (cm, left-handed).
	static FVector SimToWorld(const FPVSim::Vec3& V);
	static FPVSim::Vec3 WorldToSim(const FVector& V);
	static FQuat SimToWorld(const FPVSim::Quat& Q);
	static FPVSim::Quat WorldToSim(const FQuat& Q);

	FPVSim::Params Params;
	FPVSim::State Sim;
	double StepAccumulator = 0.0;
	bool bParamsDirty = true;
	FDelegateHandle SettingsChangedHandle;

	// Arming state (local pilot)
	bool bArmed = false;
	bool bArmSwitchSeenOff = false;
	bool bMode3D = false;
	bool bTurtle = false;
	EFPVFlightMode Mode = EFPVFlightMode::Acro;
	FText ArmBlockReason;
	float FlightTime = 0.0f;
	float LastThrottle = 0.0f;
	FString InputSourceName;

	// Link / failsafe
	float LinkQuality = 1.0f;
	float SignalTraceTimer = 0.0f;
	bool bLineOfSight = true;
	float FailsafeTimer = 0.0f;
	bool bFailsafe = false;
	FVector HomeLocation = FVector::ZeroVector;

	// Ground contact
	float ContactTimer = 0.0f;
	bool bInContact = false;

	// Networking
	float SendTimer = 0.0f;
	TArray<FFPVNetState> Buffer;
	double PlaybackTime = 0.0;
	bool bServerSimActive = false;
	float RestTimer = 0.0f;
	float ServerTime = 0.0f;
	float ChargeCheckTimer = 0.0f;
	float ChargerGraceTimer = 0.0f;	// accept charging reports shortly after leaving the range
	float ChargePublishTimer = 0.0f;

	// Local view
	bool bLocallyPiloting = false;
	TWeakObjectPtr<APawn> PilotPawn;
	TSharedPtr<SFPVOSD> OSD;
	EFPVCameraMode AppliedCameraMode = EFPVCameraMode::FPV;

	// Visual prop spin
	float PropAngle[4] = { 0, 0, 0, 0 };
};
