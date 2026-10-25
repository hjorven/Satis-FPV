#include "FPVPilotLink.h"

#include "Content/FPVContent.h"
#include "EngineUtils.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FPVDroneActor.h"
#include "FPVDroneModule.h"
#include "FPVLocalSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#define LOCTEXT_NAMESPACE "FPVDrone"

namespace FPVPilotLinkLocal
{
	constexpr float PickUpReachCm = 500.0f;
	constexpr float ClaimReachCm = 1000.0f;
}

AFPVPilotLink::AFPVPilotLink()
{
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	bAlwaysRelevant = false;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = false;
	NetUpdateFrequency = 1.0f;
}

APlayerController* AFPVPilotLink::GetPC() const
{
	return Cast<APlayerController>(GetOwner());
}

AFPVPilotLink* AFPVPilotLink::FindLocal(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<AFPVPilotLink> It(const_cast<UWorld*>(World)); It; ++It)
	{
		const APlayerController* PC = It->GetPC();
		if (PC != nullptr && PC->IsLocalController())
		{
			return *It;
		}
	}
	return nullptr;
}

AFPVPilotLink* AFPVPilotLink::FindFor(const APlayerController* PC)
{
	if (PC == nullptr || PC->GetWorld() == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<AFPVPilotLink> It(PC->GetWorld()); It; ++It)
	{
		if (It->GetPC() == PC)
		{
			return *It;
		}
	}
	return nullptr;
}

AFPVDroneActor* AFPVPilotLink::FindOwnedDrone() const
{
	const APlayerController* PC = GetPC();
	for (TActorIterator<AFPVDroneActor> It(GetWorld()); It; ++It)
	{
		if (It->GetOwner() == PC)
		{
			return *It;
		}
	}
	return nullptr;
}

AFPVDroneActor* AFPVPilotLink::FindNearestDrone(float MaxDistanceCm, bool bOnlyFree) const
{
	const APlayerController* PC = GetPC();
	const APawn* Pawn = PC != nullptr ? PC->GetPawn() : nullptr;
	if (Pawn == nullptr)
	{
		return nullptr;
	}
	AFPVDroneActor* Best = nullptr;
	double BestDistance = MaxDistanceCm;
	for (TActorIterator<AFPVDroneActor> It(GetWorld()); It; ++It)
	{
		if (It->IsPiloted() && It->GetOwner() != PC)
		{
			continue; // someone else is flying it
		}
		if (bOnlyFree && It->GetOwner() != nullptr && It->GetOwner() != PC)
		{
			continue;
		}
		const double Distance = FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = *It;
		}
	}
	return Best;
}

void AFPVPilotLink::Server_ToggleGoggles_Implementation()
{
	APlayerController* PC = GetPC();
	if (PC == nullptr)
	{
		return;
	}

	if (AFPVDroneActor* Owned = FindOwnedDrone())
	{
		Owned->ServerSetPiloted(!Owned->IsPiloted());
		return;
	}

	// Drones from a loaded save have no owner yet: claim one close by.
	if (AFPVDroneActor* Free = FindNearestDrone(FPVPilotLinkLocal::ClaimReachCm, true))
	{
		Free->SetOwner(PC);
		Free->ServerSetPiloted(true);
		return;
	}

	LaunchFromInventory();
}

void AFPVPilotLink::LaunchFromInventory()
{
	APlayerController* PC = GetPC();
	AFGCharacterPlayer* Character = PC != nullptr ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
	if (Character == nullptr)
	{
		return;
	}
	UFGInventoryComponent* Inventory = Character->GetInventory();
	const TSubclassOf<UFGItemDescriptor> DroneItem = UFPVItemDescriptor_Drone::StaticClass();
	if (Inventory == nullptr || !Inventory->HasItems(DroneItem, 1))
	{
		Client_Notify(LOCTEXT("NoDrone", "No FPV drone in your inventory. Unlock it in the AWESOME Shop and craft it in the Equipment Workshop."));
		return;
	}
	Inventory->Remove(DroneItem, 1);

	const FRotator Facing(0.0f, Character->GetActorRotation().Yaw, 0.0f);
	const FVector Location = Character->GetActorLocation() + Facing.Vector() * 150.0f + FVector(0.0f, 0.0f, 20.0f);

	FActorSpawnParameters Params;
	Params.Owner = PC;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AFPVDroneActor* Drone = GetWorld()->SpawnActor<AFPVDroneActor>(AFPVDroneActor::StaticClass(), Location, Facing, Params);
	if (Drone == nullptr)
	{
		Inventory->AddStack(FInventoryStack(1, DroneItem), true);
		return;
	}
	AFPVServerManager* Manager = AFPVServerManager::Find(GetWorld());
	const float StateOfCharge = Manager != nullptr ? Manager->PopStoredCharge(PC) : 1.0f;
	Drone->ServerInitialize(FTransform(Facing, Drone->GetActorLocation()), StateOfCharge);
	Drone->ServerSetPiloted(true);
	UE_LOG(LogFPVDrone, Log, TEXT("%s launched a drone"), *PC->GetName());
}

void AFPVPilotLink::Server_PickUp_Implementation()
{
	APlayerController* PC = GetPC();
	AFGCharacterPlayer* Character = PC != nullptr ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
	if (Character == nullptr || Character->GetInventory() == nullptr)
	{
		return;
	}
	AFPVDroneActor* Drone = FindNearestDrone(FPVPilotLinkLocal::PickUpReachCm, true);
	if (Drone == nullptr)
	{
		Client_Notify(LOCTEXT("NothingToPickUp", "No drone within reach."));
		return;
	}
	PickUp(Drone);
}

bool AFPVPilotLink::PickUp(AFPVDroneActor* Drone)
{
	APlayerController* PC = GetPC();
	AFGCharacterPlayer* Character = PC != nullptr ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
	if (Drone == nullptr || Character == nullptr || Character->GetInventory() == nullptr)
	{
		return false;
	}
	if (Drone->GetOwner() != nullptr && Drone->GetOwner() != PC)
	{
		Client_Notify(LOCTEXT("NotYourDrone", "This drone belongs to another pilot."));
		return false;
	}
	const TSubclassOf<UFGItemDescriptor> DroneItem = UFPVItemDescriptor_Drone::StaticClass();
	if (Character->GetInventory()->AddStack(FInventoryStack(1, DroneItem), false) == 0)
	{
		Client_Notify(LOCTEXT("InventoryFull", "Inventory full."));
		return false;
	}
	if (Drone->IsPiloted())
	{
		Drone->ServerSetPiloted(false);
	}
	if (AFPVServerManager* Manager = AFPVServerManager::Find(GetWorld()))
	{
		Manager->PushStoredCharge(PC, Drone->GetStateOfCharge());
	}
	Drone->Destroy();
	return true;
}

void AFPVPilotLink::Client_Notify_Implementation(const FText& Message)
{
	if (UFPVLocalSubsystem* Local = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UFPVLocalSubsystem>() : nullptr)
	{
		Local->ShowToast(Message);
	}
}

// --- Server manager ---------------------------------------------------------------------

AFPVServerManager::AFPVServerManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	bReplicates = false;
}

AFPVServerManager* AFPVServerManager::Find(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	TActorIterator<AFPVServerManager> It(const_cast<UWorld*>(World));
	return It ? *It : nullptr;
}

FString AFPVServerManager::PlayerKey(const APlayerController* PC)
{
	const APlayerState* PlayerState = PC != nullptr ? PC->GetPlayerState<APlayerState>() : nullptr;
	if (PlayerState != nullptr && PlayerState->GetUniqueId().IsValid())
	{
		// Call ToString() on the underlying net id (virtual, resolved via vtable) instead of
		// FUniqueNetIdWrapper::ToString(), whose symbol no linked module exports here.
		return PlayerState->GetUniqueId()->ToString();
	}
	return PC != nullptr ? PC->GetName() : FString();
}

void AFPVServerManager::PushStoredCharge(const APlayerController* PC, float StateOfCharge)
{
	StoredCharges.FindOrAdd(PlayerKey(PC)).Add(FMath::Clamp(StateOfCharge, 0.0f, 1.0f));
}

float AFPVServerManager::PopStoredCharge(const APlayerController* PC)
{
	TArray<float>* Charges = StoredCharges.Find(PlayerKey(PC));
	if (Charges == nullptr || Charges->Num() == 0)
	{
		return 1.0f; // freshly crafted
	}
	return Charges->Pop();
}

void AFPVServerManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	for (auto It = Links.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			if (AFPVPilotLink* Link = It->Value.Get())
			{
				Link->Destroy();
			}
			It.RemoveCurrent();
		}
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC == nullptr || Links.Contains(PC))
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.Owner = PC;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		if (AFPVPilotLink* Link = GetWorld()->SpawnActor<AFPVPilotLink>(AFPVPilotLink::StaticClass(), FTransform::Identity, Params))
		{
			Links.Add(PC, Link);
		}
	}
}

#undef LOCTEXT_NAMESPACE
