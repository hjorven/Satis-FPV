// Client-side glue: hotkeys, config menu and short notifications.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FPVLocalSubsystem.generated.h"

class APlayerController;
class SFPVConfigMenu;
class SWidget;

UCLASS()
class FPVDRONE_API UFPVLocalSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void ShowToast(const FText& Message, float Seconds = 4.0f);

	void OpenMenu();
	void CloseMenu();
	bool IsMenuOpen() const { return Menu.IsValid(); }

private:
	APlayerController* GetLocalPC() const;
	static bool IsHoldingRadio(const APlayerController* PC);
	void HideToast();

	TSharedPtr<SFPVConfigMenu> Menu;
	TSharedPtr<SWidget> Toast;
	float ToastTimeLeft = 0.0f;
	float ConfigSyncTimer = 0.0f;
};
