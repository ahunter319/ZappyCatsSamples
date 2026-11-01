// Copyright Alyssa Hunter 2024.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../../Library/SCJsonFunctionLibrary.h"
#include "GalleryWidget.generated.h"

class UGalleryImageWidget;
class UScrollBox;
class UWrapBox;

UCLASS()
class ZAPPYCATS_API UGalleryWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:

	// HELPERS	
	UFUNCTION(BlueprintCallable)
	FScreenshotsContainer GetScreenshotsFromJson();

	void SaveEditedContainerToJson(const FScreenshotsContainer& Container);

	// MAIN FEATURES
	UFUNCTION(BlueprintCallable)
	void RemoveScreenshotFromLog(FString FilePath);

	UFUNCTION(BlueprintCallable)
	void FavoriteScreenshot(const FScreenshotInfo& Info);

	UFUNCTION(BlueprintCallable)
	void GetFavoriteScreenshots(TArray<FScreenshotInfo>& OutScreenshots);

	// CALLBACKS
	UFUNCTION()
	void UpdateFavoriteStatusRequested(const FScreenshotInfo& ToUpdate);

	UFUNCTION()
	void DeleteScreenshotRequested(FString FilePath);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void ToggleNoPhotosHint(bool bShowHint);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	void PopulateScreenshotMatrix();
	void BindToScreenshotEvents();

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<UGalleryImageWidget*> GalleryImages;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UGalleryImageWidget> ImageWidgetClass;

	// All screenshots the player has taken
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<FScreenshotInfo> Screenshots;

	// The Wrapbox that will hold all the image Widgets
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (BindWidget))
	UWrapBox* ImageGrid;
};

