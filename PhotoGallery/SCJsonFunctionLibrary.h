// Copyright Alyssa Hunter 2024.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SharedPointer.h"
#include "SCJsonFunctionLibrary.generated.h"

class FJsonObject;

USTRUCT(BlueprintType)
struct FScreenshotInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString FilePath = FString();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bFavorited = false;
};

USTRUCT(BlueprintType)
struct FScreenshotsContainer
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FScreenshotInfo> Screenshots;
};

// BlueprintFunctionLibrary Subclass for working with txt and Json files. Includes helper functions for the game's screenshot-specific behavior. File IO and json read/write functions borrowed and/or adapted from Alex Quevillon's youtube tutorials on reading/writing Json. Thank you Alex!
UCLASS()
class ZAPPYCATS_API USCJsonFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// File IO
	UFUNCTION(BlueprintCallable, Category = FileIO)
	static FString ReadStringFromFile(FString FilePath, bool& bOutSuccess, FString& OutInfoMessage);

	UFUNCTION(BlueprintCallable, Category = FileIO)
	static void WriteStringToFile(FString FilePath, FString String, bool& bOutSuccess, FString& OutInfoMessage);

	// JSON
	static TSharedPtr<FJsonObject> ReadJson(FString JsonFilePath, bool& bOutSuccess, FString& OutInfoMessage); // can't have FJsonObject or TSharedPtr with UFUNCTION :(
	
	static void WriteJson(FString JsonFilePath, TSharedPtr<FJsonObject> JsonObject, bool& bOutSuccess, FString& OutInfoMessage);

	UFUNCTION(BlueprintCallable, Category = Json)
	static FScreenshotsContainer ReadStructFromJsonFile(FString JsonFilePath, bool& bOutSuccess, FString& OutInfoMessage);

	UFUNCTION(BlueprintCallable, Category = Json)
	static void WriteStructureToJsonFile(FString JsonFilePath, FScreenshotsContainer Struct, bool& bOutSuccess, FString& OutInfoMessage);

	// SCREENSHOTS
	UFUNCTION(BlueprintPure, Category = Screenshots)
	static FString GetScreenshotsFolderPath();
};
