// Copyright Alyssa Hunter 2024.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Photographer.generated.h"

class UFloatingPawnMovement;
class UCameraComponent;
class USphereComponent;
class UInputMappingContext;
class UInputAction;
class AExploreCat;
struct FInputActionValue;


UCLASS()
class ZAPPYCATS_API APhotographer : public APawn
{
	GENERATED_BODY()

public:
	APhotographer();

	virtual void Tick(float DeltaTime) override;
	
	virtual void PossessedBy(AController* NewController) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void UseGlobalMouseSettings();

	UFUNCTION(BlueprintCallable)
	void UpdateMouseSettings(float HSens, float VSens, bool bInvertH, bool bInvertV);

protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	float GetDistanceFromTether();
	void UpdateHUDDistance();
	void CheckShouldToggleTetherWarning();

	void Move(const FInputActionValue& Value);
	void FixOvershoot();
	void Ascend();
	void Descend();
	void Look(const FInputActionValue& Value);
	void Zoom(const FInputActionValue& Value); // scroll wheel
	void ResetCamera(); // scroll button resets roll and aperature
	void RollLeft(const FInputActionValue& Value);
	void RollRight(const FInputActionValue& Value);
	void Capture();

	void RegisterScreenshot(FString ImageFilePath);

	UFUNCTION(BlueprintImplementableEvent)
	void PauseGame();

	// INPUT
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* AscendAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* DescendAction;
	
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* LookAction;
	
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* AperatureAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* RollLeftAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* RollRightAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* ResetAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* CaptureAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* PauseAction;

	// STRUCTURE
	UPROPERTY(EditAnywhere)
	TObjectPtr<UFloatingPawnMovement> FloatingPawnMovement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere)
	TObjectPtr<USphereComponent> SphereComp;

	UPROPERTY(EditAnywhere)
	TSubclassOf<AExploreCat> CatClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	AExploreCat* CatCharacter;


	// PHOTOGRAPHER PROPERTIES

	UPROPERTY(EditAnywhere, Category = "Photographer")
	float TetherRadius = 2500.f;

	UPROPERTY(EditAnywhere, Category = "Photographer")
	float TetherAcceptance = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Photographer")
	float BaseFOV = 30.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Photographer")
	float FOVMultiplier = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Photographer")
	float FOVMin = 5.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Photographer")
	float FOVMax = 170.f;

	UPROPERTY(EditAnywhere, Category = "Input")
	int AscentSpeed = 75;

	UPROPERTY(EditAnywhere, Category = "Input")
	float RollSensitivity = 10.f;


	// GLOBAL PROPERTIES
	UPROPERTY(EditAnywhere, Category = "Input")
	float LookUpSensitivity = 30.f;

	UPROPERTY(EditAnywhere, Category = "Input")
	float LookSideSensitivity = 40.f;

	UPROPERTY(EditAnywhere, Category = "Input")
	bool InvertVerticalAxis = true;

	UPROPERTY(EditAnywhere, Category = "Input")
	bool InvertHorizontalAxis = false;
};
