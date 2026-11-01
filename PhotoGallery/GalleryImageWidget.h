// Copyright Alyssa Hunter 2024.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../../Library/SCJsonFunctionLibrary.h"
#include "GalleryImageWidget.generated.h"

struct FScreenshotInfo;
class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUpdateFavoriteStatus, const FScreenshotInfo&, AffectedScreenshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeleteScreenshotRequested, FString, ToDeletePath);


UCLASS()
class ZAPPYCATS_API UGalleryImageWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	// UI FUNCTIONS
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void ProcessScreenshotInfo(const FScreenshotInfo& Info);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void FavoriteButtonStyle();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void UnfavoriteButtonStyle();

	// DELEGATES
	UPROPERTY(BlueprintAssignable)
	FOnUpdateFavoriteStatus OnUpdateFavoriteStatus;

	UPROPERTY(BlueprintAssignable)
	FOnDeleteScreenshotRequested OnDeleteScreenshotRequested;

	// BUTTONS
	UFUNCTION()
	void OnFavoriteClicked();

	UFUNCTION()
	void OnDeleteClicked();

	// PROPERTIES/VARIABLES
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FScreenshotInfo ScreenshotInfo;

protected:

	virtual void NativeConstruct() override;

	// WIDGETS
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UButton* Button_Image;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UButton* Button_Heart;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UButton* Button_Delete;
};
