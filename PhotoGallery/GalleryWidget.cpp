// Copyright Alyssa Hunter 2024.


#include "GalleryWidget.h"
#include "GalleryImageWidget.h"

#include "Kismet/GameplayStatics.h"
#include "Components/WrapBox.h"

#pragma region Widget
void UGalleryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	Screenshots = GetScreenshotsFromJson().Screenshots;
}

void UGalleryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ToggleNoPhotosHint(Screenshots.Num() == 0);

	PopulateScreenshotMatrix();
	BindToScreenshotEvents();
}

void UGalleryWidget::PopulateScreenshotMatrix()
{
	for (FScreenshotInfo& Info : Screenshots)
	{
		if (UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			UGalleryImageWidget* NewImage = CreateWidget<UGalleryImageWidget>(
				UGameplayStatics::GetPlayerController(GetWorld(), 0),
				ImageWidgetClass
			);
			if (!NewImage) return;
			GalleryImages.Add(NewImage);
			ImageGrid->AddChild(NewImage);
			NewImage->ProcessScreenshotInfo(Info);
		}
	}
}

void UGalleryWidget::BindToScreenshotEvents()
{
	for (UGalleryImageWidget*& Image : GalleryImages)
	{
		Image->OnUpdateFavoriteStatus.AddDynamic(this, &UGalleryWidget::UpdateFavoriteStatusRequested);

		Image->OnDeleteScreenshotRequested.AddDynamic(this, &UGalleryWidget::DeleteScreenshotRequested);
	}
}

void UGalleryWidget::UpdateFavoriteStatusRequested(const FScreenshotInfo& ToUpdate)
{
	FScreenshotInfo NewInfo = ToUpdate;
	NewInfo.bFavorited = !NewInfo.bFavorited;
	FavoriteScreenshot(NewInfo);
}

void UGalleryWidget::DeleteScreenshotRequested(FString FilePath)
{
	RemoveScreenshotFromLog(FilePath);

	for (int32 i = 0; i < GalleryImages.Num(); i++)
	{
		if (GalleryImages[i]->ScreenshotInfo.FilePath == FilePath)
		{
			ImageGrid->RemoveChild(GalleryImages[i]);
			Screenshots.RemoveAt(i);
			GalleryImages.RemoveAt(i);
		}
	}

	ToggleNoPhotosHint(Screenshots.Num() == 0);
}
#pragma endregion Widget

#pragma region JSON
FScreenshotsContainer UGalleryWidget::GetScreenshotsFromJson()
{
	// Helper function to get the main struct from json file
	bool ReadSuccess;
	FString ReadMsg;
	FString ScreenshotsFolder = USCJsonFunctionLibrary::GetScreenshotsFolderPath();
	FString JsonFilePath = FString::Printf(TEXT("%s/Screenshots.json"), *ScreenshotsFolder);

	return USCJsonFunctionLibrary::ReadStructFromJsonFile(JsonFilePath, ReadSuccess, ReadMsg);
}

void UGalleryWidget::SaveEditedContainerToJson(const FScreenshotsContainer& Container)
{
	// Helper/Wrapper to save edits to container 
	FString ScreenshotsFolder = USCJsonFunctionLibrary::GetScreenshotsFolderPath();
	FString JsonFilePath = FString::Printf(TEXT("%s/Screenshots.json"), *ScreenshotsFolder);
	bool ReadSuccess;
	FString ReadMsg;
	USCJsonFunctionLibrary::WriteStructureToJsonFile(JsonFilePath, Container, ReadSuccess, ReadMsg);
}

void UGalleryWidget::RemoveScreenshotFromLog(FString FilePath)
{
	if (FilePath.IsEmpty()) return;

	FScreenshotsContainer AllScreenshots = GetScreenshotsFromJson();
	if (AllScreenshots.Screenshots.IsEmpty()) return;

	// Remove the passed filepath from struct
	for (int32 i = 0; i < AllScreenshots.Screenshots.Num(); i++)
	{
		if (AllScreenshots.Screenshots[i].FilePath == FilePath)
		{
			//AllScreenshots.Screenshots.RemoveAtSwap(i);
			AllScreenshots.Screenshots.RemoveAt(i);
		}
	}
	SaveEditedContainerToJson(AllScreenshots);

	// Now delete the file. First making sure it's both valid and actually exists
	if (FPaths::ValidatePath(FilePath) && FPaths::FileExists(FilePath))
	{
		IFileManager::Get().Delete(*FilePath);
	}
}

void UGalleryWidget::FavoriteScreenshot(const FScreenshotInfo& Info)
{
	if (Info.FilePath.IsEmpty()) return;

	FScreenshotsContainer AllScreenshots = GetScreenshotsFromJson();
	if (AllScreenshots.Screenshots.IsEmpty()) return;

	// Change Favorited status of given file
	for (FScreenshotInfo& ToChange : AllScreenshots.Screenshots)
	{
		if (ToChange.FilePath == Info.FilePath)
		{
			ToChange.bFavorited = Info.bFavorited;
		}
	}

	SaveEditedContainerToJson(AllScreenshots);
}

void UGalleryWidget::GetFavoriteScreenshots(TArray<FScreenshotInfo>& OutScreenshots)
{
	FScreenshotsContainer AllScreenshots = GetScreenshotsFromJson();
	if (AllScreenshots.Screenshots.IsEmpty()) return;

	// Change Favorited status of given file
	for (FScreenshotInfo Info : AllScreenshots.Screenshots)
	{
		if (Info.bFavorited)
		{
			OutScreenshots.Add(Info);
		}
	}
}
#pragma endregion JSON

