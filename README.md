# Zappy Cats - Code Sampler
<small>*"Zappy Cats" is a 3D puzzle platformer where you play as a cat that shoots lasers. It's built with Unreal Engine 5 and C++, and currently available for download on [Steam](https://store.steampowered.com/app/4291920/Zappy_Cats/).*</small>

This repo serves as a sampling of the code from my game "Zappy Cats". I designed and coded it myself without any AI, and any snippets borrowed/modified from the web have been marked and credited to the best of my knowledge. 

It took around 14 months of full-time work to build the entire game, which included all of the code, scripting, level design, sound design, and most of the 3D assets.

Rather than upload the entire codebase, I've opted to write a few feature highlights. You can read about them below or check out the code by browsing the files in this repo. If you'd like a more game-design-focussed perspective, check out my [portfolio site](https://ahunter319.github.io/).
___
### Jump to...
* [Photographer Mode](#photographer-mode)
* [Photo Gallery](#photo-gallery)
* [Auto-Shooter Class](#auto-shooter-class)
* [Enemy Rats](#enemy-rats)

___
___
### Photographer Mode

The Photographer Mode is a special gameplay mode where Players can switch from controlling their cat to controlling a camera around the level to take pictures. 

To do this, I created the Photographer class, which extends the Pawn class and includes the Camera and Floating Pawn Movement components. The Photographer is anchored to the cat's location so that Players can't cheat and use the Photographer mode to fly around obstacles.

```c++
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
```

To take a photo, the Player can press "C" or the "Left Mouse Button". A screenshot is captured and saved to an auto-generated file name, and then the RegisterScreenshot function is called so that we can store its information for easy lookup later to populate the [Photo Gallery](#photo-gallery).
```c++
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
```
Once we have a screenshot, we create an FScreenshotInfo struct using its file path and add it to the Screenshots.json file for quick look-up. Players are able to view, favorite, and delete their screenshots in the Photo Gallery, so the JSON file is very helpful.
```c++
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
```
Check out more of the full Photographer code in the "Photographer" folder. For reading and writing to JSON files I created a Blueprint Function Library called SCJsonFunctionLibrary with the help of Alex Quevillon's Youtube series on working with JSON in Unreal. You can browse the code in the "PhotoGallery" folder. 


___
___
### Photo Gallery

Players can view their screenshots, favorite, or delete them from the Photo Gallery in the game. It is accessed from the main menu and displays as a grid of images 4 across and as many down as needed. The grid is able to grow or shrink dynamically as the Player takes or deletes photos. 

To do this I created the Gallery Widget class to dynamically populate and display all the photos and a Gallery Image Widget class for the individual photos. Both extend UE5's UserWidget class to take advantage of its built-in UI functions.

The Gallery Widget gets the screenshots from the Screenshots.json file and then creates an instance of the Gallery Image Widget class for each screenshot, passing them the relevant info for displaying their image and favorite status. The Gallery Widget also handles updating the JSON file when a photo is deleted or its favorite status changes. Clicking on a Gallery Image Widget will pull up a larger version of it with an Exit button.

```c++
FScreenshotsContainer UGalleryWidget::GetScreenshotsFromJson()
{
	// Helper function to get the main struct from json file
	bool ReadSuccess;
	FString ReadMsg;
	FString ScreenshotsFolder = USCJsonFunctionLibrary::GetScreenshotsFolderPath();
	FString JsonFilePath = FString::Printf(TEXT("%s/Screenshots.json"), *ScreenshotsFolder);

	return USCJsonFunctionLibrary::ReadStructFromJsonFile(JsonFilePath, ReadSuccess, ReadMsg);
}
```
Once it gathers all the screenshots from the JSON file, it can populate the gallery with them and display their favorite status. PopulateScreenshotMatrix() is called in NativeConstruct(). 
```c++
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
```
Any changes made in the Photo Gallery are saved via the helper function SaveEditedContainerToJson(). Note that the array of screenshots must be contained in a struct called FScreenshotsContainer in order for it to be processed/understood by JSON.
```c++
void UGalleryWidget::SaveEditedContainerToJson(const FScreenshotsContainer& Container)
{
	// Helper/Wrapper to save edits to container 
	FString ScreenshotsFolder = USCJsonFunctionLibrary::GetScreenshotsFolderPath();
	FString JsonFilePath = FString::Printf(TEXT("%s/Screenshots.json"), *ScreenshotsFolder);
	bool ReadSuccess;
	FString ReadMsg;
	USCJsonFunctionLibrary::WriteStructureToJsonFile(JsonFilePath, Container, ReadSuccess, ReadMsg);
}
```
Check out the rest of the Photo Gallery code in the "PhotoGallery" folder.

___
___
### Auto-Shooter Class

The AutoShooter Class is used for stationary objects that should sense the Player and fire at them while they are visible and in range. 

It was used in the game to create various types of canons that shoot explosive canonballs at the Player. It could also be used for firing arrows or darts for different traps and obstacles. 

This class inherits from UE5's Pawn class and implements the Saveable Interface I created for easily saving and loading an object's state. 

AutoShooter uses UE5's Pawn Sensing Component for "seeing" the Player, as well as the Health Widget Component I created for displaying an actor's health. Several other classes make use of the Health Widget Component. 

This class can take damage and be destroyed. The game saves the "health" of each AutoShooter and whether or not they have been destroyed so that Players returning to a session do not have to redo their work. 

AutoShooter's main purpose is to fire projectiles at the Player. To do this, we first bind a callback to the Pawn Sensing Component's "OnSeePawn" delegate. This is done in the BeginPlay() function, which is called at the beginning of a session.

```c++
void AAutoShooter::BeginPlay()
{
    Super::BeginPlay();
    
    OnTakeAnyDamage.AddDynamic(this, &AAutoShooter::ReceiveDamage);

    if (PawnSensing) PawnSensing->OnSeePawn.AddDynamic(this, &AAutoShooter::PawnSeen);

    Health = MaxHealth;
    HealthWidgetComponent->SetHealthPercent(Health / MaxHealth);
    ShouldShowHealthBar(false);
}

```
The Player class is a Pawn through its inheritance hierarchy, so AutoShooter will be able to "see" it. The AttackRange variable determines how far it can see and is exposed to Blueprints so that the designer can adjust it easily without recompiling any code. 
```c++
void AAutoShooter::PawnSeen(APawn* SeenPawn)
{
    if (Target) return; // don't override an active Target
    if (Health <= 0.f) return;

    if (SeenPawn->ActorHasTag("Player") && !SeenPawn->ActorHasTag("Dead"))
    {
        Target = SeenPawn;
    }
    // don't override current attack loop
    if (GetWorldTimerManager().IsTimerActive(AttackTimer)) return;
    // start new attack loop
    if (IsValid(Target)) WaitForNextAttack();
}

```
Once it has "seen" the Player, it begins an attack cycle where it fires projectiles at random intervals. The min and max values for these intervals are also exposed to blueprints for easy adjustments. 

```c++
void AAutoShooter::WaitForNextAttack()
{
	float FireRate = FMath::RandRange(FireRateMin, FireRateMax);
	PrepareToFireFX(); // Implemented in Blueprints
	GetWorldTimerManager().SetTimer(AttackTimer, this, &AAutoShooter::Attack, FireRate);
}
```
We first check if firing is possible using the helper functions I wrote, then spawn the projectile. Once it is spawned, it takes care of its movement, collision, and subsequent damage caused by a successful hit. The AutoShooter merely spawns it and points it in the right direction.

```c++
void AAutoShooter::Attack()
{
    bool bCanFire = IsValid(ProjectileClass) && TargetInRange() && TargetWithinArch() && HasLineOfSight();

    if (!bCanFire)
    {
        Target = nullptr;
        ShouldShowHealthBar(false); 
        CancelFX(); // Implemented in Blueprints
        return;
    }

    // Implemented in Blueprints
    FireFX();

    // Spawn projectile
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    FVector Start = MuzzleLocation->GetComponentLocation();
    FVector TargetLocation = Target->GetActorLocation() - Start;
    FRotator TargetRotation = TargetLocation.Rotation();

    AProjectile* SpawnedProjectile = GetWorld()->SpawnActor<AProjectile>(ProjectileClass, Start, TargetRotation, SpawnParams);

    // If target is still valid and alive, continue attack cycle
    if (IsValid(Target) && !Target->ActorHasTag(FName("Dead"))) WaitForNextAttack();
}
```
The helper functions above, the saving and loading functions, and the rest of the code for the AutoShooter class are available to browse in the "AutoShooter" folder. 


___
___
### Enemy Rats

The Rat class is used for all of the mobile enemies in the game. Their main purpose is to patrol between a set of locations and chase and then attack the Player when they sense it. Each Rat is controlled by a Manager and is able to communicate to the Manager and the other rats in its Unit.

The Rat class inherits from UE5's Character class and implements the Combat Interface I created for easily rotating the character mesh towards its combat target. It also uses my custom Combat and Health Components for attacking and receiving damage.

Rats are meant to work as a pack, so they communicate with each other through a Manager class. Each Rat has custom delegates they can broadcast for various events, such as when they spot the Player or when they take critical damage. The Manager is the class that subscribes to these delegates and makes decisions on which rats to inform or dispatch.  

The enemy behavior loop is implemented by extending UE5's built-in Tick function. It is called every 'tick' of the game for each rat so that they can make real-time decisions.

```c++
void ARatCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HealthComp && HealthComp->IsDead()) return;
	if (!IsUnoccupied()) return;

	// Update Widgets
	AdjustWidgetScale();
	SetStateWidgetValues();

    // Track Player if visible
	UpdatePlayerLastKnownLocation();

    // Main Enemy behavior
	DetermineNextEnemyAction();

	// Visualize the Rat's travel boundaries
	if (bDebugBoundaries)
	{
		VisualizeBoundaries();
	}
}
```
The Rat's enemy behavior loop relies on the EnemyState enum variable to determine which actions to take. The main loop is to patrol between locations, and if the Player is spotted, to either chase or attack depending on how far away it is. When the Player is spotted, the Rat will alert the Manager so it can tell the other rats where the Player is. The Manager has complete power to override a Rat's current task, and any order given by the Manager is always given priority. 
```c++
void ARatCharacter::DetermineNextEnemyAction()
{
	// Don't interrupt an order from Manager
	if (IsFollowingOrder()) return;

	if (EnemyState == EEnemyState::EES_Searching)
	{
		// If we're not already moving to a new search location, play "searching" animation
		if (!IsMoving())
		{
			UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
			if (AnimInstance && !AnimInstance->IsAnyMontagePlaying()) PlaySearchAnimation();
		}
	}
	// Rat is Chasing/Attacking/Engaged/FollowingOrder
	else if (EnemyState > EEnemyState::EES_Searching)
	{
		CheckCombatTarget();
	}
	// Rat is in Default or Patrolling state and is already at it's current patrol target (so we don't interrupt moving between targets before we get there)
	else if (IsAtTarget())
	{
		CheckPatrolTarget();
	}
	// Rat is in Default or Patrolling state and hasn't yet reached PatrolTarget
	else
	{
		MoveToPatrol();
	}
}
```
The Rat's combat logic is mainly implemented in CheckCombatTarget(). Note that because the Rat class uses the Pawn Sensing Componet, it is able to "see" and "hear" any Pawns. I added the ability to "smell" the Player by giving them a timed "odor" effect. If the Player has this effect, the Rat will be able to smell it. 
```c++
void ARatCharacter::CheckCombatTarget()
{
	if (!IsValid(TargetPlayer))
	{
		Reset();
		return;
	}

	// Player is hiding/not-visible, but if we can smell them it wont work
	if (!CanSeePlayer())
	{
		if (CanSmellPlayer())
		{
			MoveToTarget(TargetPlayer);
		}
		else
		{
			ClearAttackTimer();
			if (LastKnownTargetLocation != FVector::ZeroVector)
			{
				MoveToLocation(LastKnownTargetLocation);
				StartResetTimer();
			}
			else
			{
				Reset();
			}
		}
		return;
	}
	// If we're chasing and have traveled out of bounds, return to current patrol point.
	else if (IsChasing() && !IsWithinBounds())
	{
		Reset();
		return;
	}
	// Player is outside of Attack Range and we aren't already chasing them
	else if (!IsChasing() && !InRange(TargetPlayer, AttackRadius))
	{
		ClearAttackTimer();
		if (!IsEngaged()) ChaseTarget();
		return;
	}
	// Player is visible and within all ranges
	else if (CanAttack())
	{
		StartAttackTimer();
		return;
	}
	else if (TargetPlayer->ActorHasTag("Dead"))
	{
		OnPlayerKilled.Broadcast();
		Reset();
		return;
	}

	// Sometimes rats gets stuck 'chasing' or 'attacking' when the player has jumped (and therefore exited the navmesh which rats use to pathfind to player)
	if (EnemyState > EEnemyState::EES_Searching && !IsMoving())
	{
		if (!GetWorldTimerManager().IsTimerActive(CheckMovementTimer))
		{
			GetWorldTimerManager().SetTimer(CheckMovementTimer, this, &ARatCharacter::CheckMovementTimerFinished, 5.f);
		}
	}
}
```

The full code for the Rat class and the Rat Manager class can be browsed in the "Rat" folder. 

___
___
