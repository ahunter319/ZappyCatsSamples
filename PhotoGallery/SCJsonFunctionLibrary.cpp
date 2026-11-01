// Copyright Alyssa Hunter 2024.


#include "SCJsonFunctionLibrary.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"

FString USCJsonFunctionLibrary::ReadStringFromFile(FString FilePath, bool& bOutSuccess, FString& OutInfoMessage)
{
	// Check if the file exists
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
	{
		bOutSuccess = false;
		OutInfoMessage = FString::Printf(TEXT("Read String From File Failed - File does not exist - '%s'"), *FilePath);
		return FString();
	}

	FString ReturnString = "";

	// Try to read the file
	if (!FFileHelper::LoadFileToString(ReturnString, *FilePath))
	{
		bOutSuccess = false;
		OutInfoMessage = FString::Printf(TEXT("Read String From File Failed - Was not able to read file. Is this a text file? - '%s'"), *FilePath);
		return FString();
	}

	bOutSuccess = true;
	OutInfoMessage = FString::Printf(TEXT("Read String From File Succeeded - '%s'"), *FilePath);
	return ReturnString;
}

void USCJsonFunctionLibrary::WriteStringToFile(FString FilePath, FString String, bool& bOutSuccess, FString& OutInfoMessage)
{
	// Try to write the string into the file
	if (!FFileHelper::SaveStringToFile(String, *FilePath))
	{
		bOutSuccess = false;
		OutInfoMessage = FString::Printf(TEXT("Write String To File Failed - Was not able to write to file. Is your file read only? Is the path valid? - '%s'"), *FilePath);
		return;
	}

	bOutSuccess = true;
	OutInfoMessage = FString::Printf(TEXT("Write String To File Suceeded - '%s'"), *FilePath);
}

TSharedPtr<FJsonObject> USCJsonFunctionLibrary::ReadJson(FString JsonFilePath, bool& bOutSuccess, FString& OutInfoMessage)
{
	// Try to read file
	FString JsonString = ReadStringFromFile(JsonFilePath, bOutSuccess, OutInfoMessage);
	if (!bOutSuccess) return nullptr;

	TSharedPtr<FJsonObject> ReturnJsonObject;

	// Try to convert string to json object
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonString), ReturnJsonObject))
	{
		bOutSuccess = false;
		OutInfoMessage = FString::Printf(TEXT("Reaed Json failed - Was not able to deserialize the json string. Is it the right format? - '%s'"), *JsonString);
		return nullptr;
	}

	bOutSuccess = true;
	OutInfoMessage = FString::Printf(TEXT("Read Json Succeeded- '%s'"), *JsonFilePath);
	return ReturnJsonObject;
}

void USCJsonFunctionLibrary::WriteJson(FString JsonFilePath, TSharedPtr<FJsonObject> JsonObject, bool& bOutSuccess, FString& OutInfoMessage)
{
	FString JsonString;

	// Try to convert json object to string
	if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), TJsonWriterFactory<>::Create(&JsonString, 0)))
	{
		bOutSuccess = false;
		OutInfoMessage = FString::Printf(TEXT("Write Json Failed - Was not able to serialize the json to string. Is the JsonObject valid?"));
		return;
	}

	// Try to write json string to file
	WriteStringToFile(JsonFilePath, JsonString, bOutSuccess, OutInfoMessage);
	if (!bOutSuccess) return;

	bOutSuccess = true;
	OutInfoMessage = FString::Printf(TEXT("Write Json Succeeded- '%s'"), *JsonFilePath);
}

FScreenshotsContainer USCJsonFunctionLibrary::ReadStructFromJsonFile(FString JsonFilePath, bool& bOutSuccess, FString& OutInfoMessage)
{
	// Try to read generic json object from file
	TSharedPtr<FJsonObject> JsonObject = ReadJson(JsonFilePath, bOutSuccess, OutInfoMessage);

	if (!bOutSuccess)
	{
		return FScreenshotsContainer();
	}

	FScreenshotsContainer ScreenshotsStruct;
	// Try to convert generic json object to the desired structure
	
	if (!FJsonObjectConverter::JsonObjectToUStruct<FScreenshotsContainer>(JsonObject.ToSharedRef(), &ScreenshotsStruct))
	{
		bOutSuccess = false;
		OutInfoMessage = FString::Printf(TEXT("Read Struct Json Failed - Unable to convert the json object to desired structure"), *JsonFilePath);
		return FScreenshotsContainer();
	}

	bOutSuccess = true;
	OutInfoMessage = FString::Printf(TEXT("Read Struct Json Succeeded"), *JsonFilePath);
	return ScreenshotsStruct;
}

void USCJsonFunctionLibrary::WriteStructureToJsonFile(FString JsonFilePath, FScreenshotsContainer Struct, bool& bOutSuccess, FString& OutInfoMessage)
{
	// Try to convert struct to generic json object
	TSharedPtr<FJsonObject> JsonObject = FJsonObjectConverter::UStructToJsonObject(Struct);
	if (JsonObject == nullptr)
	{
		bOutSuccess = false;
		OutInfoMessage = FString::Printf(TEXT("Write Struct Json Failed - Was not able to convert the struct to a json object."));
		return;
	}
	WriteJson(JsonFilePath, JsonObject, bOutSuccess,OutInfoMessage);
}

FString USCJsonFunctionLibrary::GetScreenshotsFolderPath()
{
	// Screenshots will be stored in Project directory next to the main UProject/EXE based on the build type
#if WITH_EDITOR
	const FString ImageDirectory = FString::Printf(TEXT("%s/%s"), *FPaths::ProjectDir(), TEXT("Screenshots"));
	return ImageDirectory;
#else
	const FString ImageDirectory = FString::Printf(TEXT("%s/../%s"), *FPaths::ProjectDir(), TEXT("Screenshots"));
	return ImageDirectory;

#endif
	//return FString();
}


