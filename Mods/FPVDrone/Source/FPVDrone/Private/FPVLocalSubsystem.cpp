#include "FPVLocalSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Config/FPVModConfig.h"
#include "Engine/World.h"
#include "Equipment/FPVRadioEquipment.h"
#include "FGCharacterPlayer.h"
#include "FPVPilotLink.h"
#include "FPVSettings.h"
#include "GameFramework/PlayerController.h"
#include "UI/SFPVConfigMenu.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

bool UFPVLocalSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsRunningDedicatedServer() || !Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr && World->IsGameWorld();
}

void UFPVLocalSubsystem::Deinitialize()
{
	CloseMenu();
	HideToast();
	Super::Deinitialize();
}

TStatId UFPVLocalSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFPVLocalSubsystem, STATGROUP_Tickables);
}

APlayerController* UFPVLocalSubsystem::GetLocalPC() const
{
	APlayerController* PC = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr;
	return (PC != nullptr && PC->IsLocalController()) ? PC : nullptr;
}

void UFPVLocalSubsystem::Tick(float DeltaTime)
{
	if (ToastTimeLeft > 0.0f)
	{
		ToastTimeLeft -= DeltaTime;
		if (ToastTimeLeft <= 0.0f)
		{
			HideToast();
		}
	}

	ConfigSyncTimer -= DeltaTime;
	if (ConfigSyncTimer <= 0.0f)
	{
		ConfigSyncTimer = 1.0f;
		FPVModConfig::Register(GetWorld()->GetGameInstance());
		FPVModConfig::Sync(GetWorld()->GetGameInstance());
	}

	APlayerController* PC = GetLocalPC();
	if (PC == nullptr || PC->PlayerInput == nullptr)
	{
		return;
	}

	const FFPVKeyBindings& Keys = FFPVSettings::Get().Keys;
	if (PC->WasInputKeyJustPressed(FFPVSettings::ParseKey(Keys.OpenMenu)))
	{
		IsMenuOpen() ? CloseMenu() : OpenMenu();
		return;
	}
	if (IsMenuOpen())
	{
		return;
	}

	const bool bRadioClick = PC->WasInputKeyJustPressed(EKeys::LeftMouseButton) && IsHoldingRadio(PC);
	if (bRadioClick || PC->WasInputKeyJustPressed(FFPVSettings::ParseKey(Keys.ToggleGoggles)))
	{
		if (AFPVPilotLink* Link = AFPVPilotLink::FindLocal(GetWorld()))
		{
			Link->Server_ToggleGoggles();
		}
	}
	else if (PC->WasInputKeyJustPressed(FFPVSettings::ParseKey(Keys.PickUpDrone)))
	{
		if (AFPVPilotLink* Link = AFPVPilotLink::FindLocal(GetWorld()))
		{
			Link->Server_PickUp();
		}
	}
	else if (PC->WasInputKeyJustPressed(FFPVSettings::ParseKey(Keys.ToggleCamera)))
	{
		FFPVCameraSettings& Camera = FFPVSettings::Get().Camera;
		Camera.Mode = Camera.Mode == EFPVCameraMode::FPV ? EFPVCameraMode::Chase : EFPVCameraMode::FPV;
		FFPVSettings::Save();
	}
}

bool UFPVLocalSubsystem::IsHoldingRadio(const APlayerController* PC)
{
	// The character stays possessed while flying, so this also works with the goggles on.
	const AFGCharacterPlayer* Character = PC != nullptr ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
	const AFGEquipment* Equipment = Character != nullptr ? Character->GetEquipmentInSlot(EEquipmentSlot::ES_ARMS) : nullptr;
	return Equipment != nullptr && Equipment->IsA<AFPVRadioEquipment>();
}

void UFPVLocalSubsystem::OpenMenu()
{
	APlayerController* PC = GetLocalPC();
	if (IsMenuOpen() || PC == nullptr || GEngine == nullptr || GEngine->GameViewport == nullptr)
	{
		return;
	}
	Menu = SNew(SFPVConfigMenu).OnClose(FSimpleDelegate::CreateUObject(this, &UFPVLocalSubsystem::CloseMenu));
	GEngine->GameViewport->AddViewportWidgetContent(Menu.ToSharedRef(), 50);

	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(Menu);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
	PC->SetShowMouseCursor(true);
}

void UFPVLocalSubsystem::CloseMenu()
{
	if (!Menu.IsValid())
	{
		return;
	}
	if (GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
	}
	Menu.Reset();
	FFPVSettings::Save();

	if (APlayerController* PC = GetLocalPC())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}

void UFPVLocalSubsystem::ShowToast(const FText& Message, float Seconds)
{
	HideToast();
	if (GEngine == nullptr || GEngine->GameViewport == nullptr)
	{
		return;
	}
	Toast = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.0f, 120.0f, 0.0f, 0.0f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f))
			.Padding(FMargin(16.0f, 8.0f))
			[
				SNew(STextBlock)
				.Text(Message)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
				.ColorAndOpacity(FLinearColor(1.0f, 0.6f, 0.1f))
			]
		];
	GEngine->GameViewport->AddViewportWidgetContent(Toast.ToSharedRef(), 60);
	ToastTimeLeft = Seconds;
}

void UFPVLocalSubsystem::HideToast()
{
	if (Toast.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Toast.ToSharedRef());
	}
	Toast.Reset();
	ToastTimeLeft = 0.0f;
}
