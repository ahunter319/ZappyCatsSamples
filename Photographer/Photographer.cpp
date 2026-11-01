// Copyright Alyssa Hunter 2024.


#include "Photographer.h"
#include "../Character/ExploreCat.h"
#include "../Library/SCJsonFunctionLibrary.h"
#include "../Player/CatPlayerController.h"
#include "../Interface/SaveGameInterface.h"

#include "GameFramework/FloatingPawnMovement.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SphereComponent.h"

#pragma region Setup
APhotographer::APhotographer()
{
	PrimaryActorTick.bCanEverTick = true;

	// Player should not be able to clip through anything in PhotoMode
	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(SphereComp);
	SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SphereComp->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	SphereComp->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);

	FloatingPawnMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Floating Movement"));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetRootComponent());
	Camera->bUsePawnControlRotation = true;
}

void APhotographer::BeginPlay()
{
	Super::BeginPlay();

	UseGlobalMouseSettings();

	// Get the player character in level
	AActor* OutActor = UGameplayStatics::GetActorOfClass(GetWorld(), CatClass);
	if (AExploreCat* ExpCat = Cast<AExploreCat>(OutActor))
	{
		CatCharacter = ExpCat;
	}	
}

void APhotographer::Destroyed()
{
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(DefaultMappingContext);
		}
	}
}

void APhotographer::PossessedBy(AController* NewController)
{
	if (APlayerController* PlayerController = Cast<APlayerController>(NewController))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void APhotographer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APhotographer::Move);

		EnhancedInputComponent->BindAction(AscendAction, ETriggerEvent::Triggered, this, &APhotographer::Ascend);

		EnhancedInputComponent->BindAction(DescendAction, ETriggerEvent::Triggered, this, &APhotographer::Descend);

		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APhotographer::Look);

		EnhancedInputComponent->BindAction(RollLeftAction, ETriggerEvent::Triggered, this, &APhotographer::RollLeft);

		EnhancedInputComponent->BindAction(RollRightAction, ETriggerEvent::Triggered, this, &APhotographer::RollRight);
		
		EnhancedInputComponent->BindAction(AperatureAction, ETriggerEvent::Triggered, this, &APhotographer::Zoom);

		EnhancedInputComponent->BindAction(ResetAction, ETriggerEvent::Triggered, this, &APhotographer::ResetCamera);

		EnhancedInputComponent->BindAction(CaptureAction, ETriggerEvent::Completed, this, &APhotographer::Capture);
		
		EnhancedInputComponent->BindAction(PauseAction, ETriggerEvent::Completed, this, &APhotographer::PauseGame);
	}
}

void APhotographer::UseGlobalMouseSettings()
{
	// Get the current mouse settings from the save game interface
	if (ISaveGameInterface* SGI = Cast<ISaveGameInterface>(GetGameInstance()))
	{
		LookSideSensitivity = 100 * SGI->GetUserData().HorizontalSensitivity;
		LookUpSensitivity = 100 * SGI->GetUserData().VerticalSensitivity;
		InvertHorizontalAxis = SGI->GetUserData().InvertHorizontalAxis;
		InvertVerticalAxis = SGI->GetUserData().InvertVerticalAxis;
	}
}

void APhotographer::UpdateMouseSettings(float HSens, float VSens, bool bInvertH, bool bInvertV)
{
	LookSideSensitivity = 100 * HSens;
	LookUpSensitivity = 100 * VSens;
	InvertHorizontalAxis = bInvertH;
	InvertVerticalAxis = bInvertV;
}

void APhotographer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
#pragma endregion Setup

#pragma region Input
void APhotographer::Move(const FInputActionValue& Value)
{
	if (!Controller) return;
	
	const FVector2D MovementVector = Value.Get<FVector2D>();

	// get controller forward direction
	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	// get forward and right vectors
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MovementVector.X);
	AddMovementInput(RightDirection, MovementVector.Y);

	// Handle distance on HUD and if we overshot
	UpdateHUDDistance();
	CheckShouldToggleTetherWarning();
	FixOvershoot();
}

void APhotographer::Ascend()
{
	FVector UpDirection = GetActorUpVector();
	UpDirection.X = 0.f;
	UpDirection.Y = 0.f;
	AddMovementInput(UpDirection, AscentSpeed);

	UpdateHUDDistance();
	CheckShouldToggleTetherWarning();
	FixOvershoot();
}

void APhotographer::Descend()
{
	FVector DownDirection = -1 * GetActorUpVector();
	DownDirection.X = 0.f;
	DownDirection.Y = 0.f;
	AddMovementInput(DownDirection, AscentSpeed);

	UpdateHUDDistance();
	CheckShouldToggleTetherWarning();
	FixOvershoot();
}

void APhotographer::Look(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();

	float DeltaTime = UGameplayStatics::GetWorldDeltaSeconds(this);

	int32 VerticalInversion = InvertVerticalAxis ? -1 : 1;
	int32 HorizontalInversion = InvertHorizontalAxis ? -1 : 1;

	// Look Up or Down
	AddControllerPitchInput(LookVector.Y * DeltaTime * LookUpSensitivity * VerticalInversion);

	// Look Side to Side
	AddControllerYawInput(LookVector.X * DeltaTime * LookSideSensitivity * HorizontalInversion);
}

void APhotographer::Zoom(const FInputActionValue& Value)
{
	Camera->FieldOfView = FMath::Clamp(Camera->FieldOfView - Value.Get<float>() * FOVMultiplier, FOVMin, FOVMax);
}

void APhotographer::RollLeft(const FInputActionValue& Value)
{
	const float RollAmount = Value.Get<float>();
	float DeltaTime = UGameplayStatics::GetWorldDeltaSeconds(this);
	AddControllerRollInput(-1* RollAmount * DeltaTime * RollSensitivity);
}

void APhotographer::RollRight(const FInputActionValue& Value)
{
	const float RollAmount = Value.Get<float>();
	float DeltaTime = UGameplayStatics::GetWorldDeltaSeconds(this);
	AddControllerRollInput(RollAmount * DeltaTime * RollSensitivity);
}

void APhotographer::ResetCamera()
{
	// Reset Field of View
	Camera->FieldOfView = BaseFOV;

	// Reset roll to 0
	AddControllerRollInput(-GetControlRotation().Roll);
}

void APhotographer::Capture()
{
	// Tell PlayerController to play Camera animation
	if (ACatPlayerController* CatPlayerController = Cast<ACatPlayerController>(GetController()))
	{
		CatPlayerController->PlayCameraAnimation();
	}

	// Thank you Jamie Holding for the following code...
	//https://web.archive.org/web/20191230192143/https://blog.jamie.holdings/2019/09/08/taking-screenshots-in-game-in-unreal-engine-4-23/
 
	// Generate a filename based on the current date
	const FDateTime Now = FDateTime::Now();

	FString ImageDirectory = USCJsonFunctionLibrary::GetScreenshotsFolderPath();

	const FString ImageFilename = FString::Printf(TEXT("%s/Screenshot_%d%02d%02d_%02d%02d%02d_%03d.png"), *ImageDirectory, Now.GetYear(), Now.GetMonth(), Now.GetDay(), Now.GetHour(), Now.GetMinute(), Now.GetSecond(), Now.GetMillisecond());

	FScreenshotRequest::RequestScreenshot(ImageFilename, false, false);
	/// ^^^^^ End of borrowed code ^^^^^

	// save screenshot to json
	RegisterScreenshot(ImageFilename);
}

void APhotographer::RegisterScreenshot(FString ImageFilePath)
{
	// Create a new screenshotinfo to add to file
	FScreenshotInfo NewScreenshot;
	NewScreenshot.FilePath = ImageFilePath;
	NewScreenshot.bFavorited = false;

	// Try to access/read Screenshots struct from Json 
	bool ReadSuccess;
	FString ReadMsg;
	FString ScreenshotsFolder = USCJsonFunctionLibrary::GetScreenshotsFolderPath();
	FString JsonFilePath = FString::Printf(TEXT("%s/Screenshots.json"), *ScreenshotsFolder);
	FScreenshotsContainer AllScreenshots = USCJsonFunctionLibrary::ReadStructFromJsonFile(JsonFilePath, ReadSuccess, ReadMsg);

	AllScreenshots.Screenshots.Add(NewScreenshot);
	USCJsonFunctionLibrary::WriteStructureToJsonFile(JsonFilePath, AllScreenshots, ReadSuccess, ReadMsg);
}

#pragma endregion Input

#pragma region Helpers
float APhotographer::GetDistanceFromTether()
{
	return (GetActorLocation() - CatCharacter->GetActorLocation()).Length();
}

void APhotographer::UpdateHUDDistance()
{
	if (ACatPlayerController* CatPlayerController = Cast<ACatPlayerController>(GetController()))
	{
		CatPlayerController->UpdateHUDTetherDistance(GetDistanceFromTether());
	}
}

void APhotographer::CheckShouldToggleTetherWarning()
{
	ACatPlayerController* CatPlayerController = Cast<ACatPlayerController>(GetController());

	if (GetDistanceFromTether() > TetherRadius - TetherAcceptance)
	{
		// tell controller to tell hud to change color of warning 
		if (CatPlayerController)
		{
			CatPlayerController->ToggleHUDDistanceWarning(true);
		}
	}
	else
	{
		if (CatPlayerController)
		{
			CatPlayerController->ToggleHUDDistanceWarning(false);
		}
	}
}

void APhotographer::FixOvershoot()
{
	// Fixes overshot movement if Photographer has moved out of bounds

	FVector FromCatToPhotographer = GetActorLocation() - CatCharacter->GetActorLocation();
	const float Distance = FromCatToPhotographer.Length();

	if (Distance > TetherRadius)
	{
		FromCatToPhotographer *= TetherRadius / Distance;
		FVector FixedLocation = CatCharacter->GetActorLocation() + FromCatToPhotographer;
		FVector LerpLocation = FMath::Lerp(GetActorLocation(), FixedLocation, 0.5f);
		SetActorLocation(LerpLocation);

		UpdateHUDDistance();
		CheckShouldToggleTetherWarning();
	}
}
#pragma endregion Helpers