// Main menu built in code (no widget blueprint needed): host / join by IP / solo test / quit.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CrankMainMenuWidget.generated.h"

class UEditableTextBox;
class UTextBlock;
class UVerticalBox;
class UButton;

UCLASS()
class CRANK_API UCrankMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	/** -CrankHost / -CrankJoin=<address> on the command line: press that button once at startup. */
	void RunCommandLineAction();

	UFUNCTION()
	void OnHost();

	UFUNCTION()
	void OnJoin();

	UFUNCTION()
	void OnSolo();

	UFUNCTION()
	void OnQuit();

	UButton* AddButton(UVerticalBox* Box, const FText& Label, FName Handler);
	UTextBlock* MakeText(const FText& Text, int32 Size, const FLinearColor& Color);

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> AddressBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;
};
