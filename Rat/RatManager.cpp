// Copyright Alyssa Hunter 2024.


#include "RatManager.h"
#include "RatCharacter.h"
#include "BaseCat.h"
#include "../HUD/HealthWidgetComponent.h"
#include "../Actors/RatUnit.h"
#include "../Interface/PlayerInterface.h"
#include "../Library/CommonFunctionLibrary.h"
#include "../Game/ExploreGameModeBase.h"

#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

#pragma region Standard
ARatManager::ARatManager()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Collision Sphere"));
	CollisionSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_GameTraceChannel4, ECollisionResponse::ECR_Overlap);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetRootComponent(CollisionSphere);

	StrongholdMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stronghold Mesh"));
	StrongholdMesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);
	StrongholdMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	StrongholdMesh->SetupAttachment(GetRootComponent());

	HealthWidgetComponent = CreateDefaultSubobject<UHealthWidgetComponent>(TEXT("Health Bar"));
	HealthWidgetComponent->SetupAttachment(GetRootComponent());

	StrongholdTagName = UCommonFunctionLibrary::GetUniqueNameWithParam(FString("Stronghold"));

	Tags.Add(FName("LaserTarget"));
}

void ARatManager::BeginPlay()
{
	Super::BeginPlay();

	OnTakeAnyDamage.AddDynamic(this, &ARatManager::ReceiveDamage);
	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ARatManager::OnSphereOverlap);
	CollisionSphere->OnComponentEndOverlap.AddDynamic(this, &ARatManager::OnSphereEndOverlap);

	Integrity = MaxIntegrity;
	HealthWidgetComponent->SetHealthPercent(Integrity / MaxIntegrity);
	ShouldShowHealthBar(false);

	// NOTE: Spawning rats is handled after GameMode determines whether or not this Manager should be destroyed
}

void ARatManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

FName ARatManager::GetNameTag()
{
	return StrongholdTagName;
}

void ARatManager::ApprovedByGameMode()
{
	// Spawn all rats per unit definitions and store them
	for (ARatUnit* Unit : RatUnits)
	{
		if (IsValid(Unit)) Unit->SetManager(this);
		SpawnAllRatsForUnit(Unit);
	}
}

void ARatManager::SaveData(UZappyCats_SaveGame& OutSaveGame)
{
	for (FManagerInfo& Info : OutSaveGame.GameProps.RatManagers)
	{
		if (Info.ManagerName == StrongholdTagName)
		{
			Info.ManagerHealth = Integrity;
			return;
		}
	}
	FManagerInfo NewInfo = FManagerInfo();
	NewInfo.ManagerHealth = Integrity;
	NewInfo.ManagerName = StrongholdTagName;
	OutSaveGame.GameProps.RatManagers.Add(NewInfo);
}

void ARatManager::LoadData(UZappyCats_SaveGame* SaveGameRef)
{
	if (!SaveGameRef) return;
	
	// If there is no saved info in SaveGameRef, we've never been to this level so everything should load its default behavoir. 
	if (SaveGameRef->GameProps.RatManagers.IsEmpty())
	{
		ApprovedByGameMode();
		return;
	}

	// Else we HAVE been to this level before and saved some data.
	for (FManagerInfo Info : SaveGameRef->GameProps.RatManagers)
	{
		if (Info.ManagerName == StrongholdTagName)
		{
			if (Info.ManagerHealth <= 0.f)
			{
				Integrity = 0.f;
				HideManager();
				// Alert GameMode so it can track level progress
				if (AExploreGameModeBase* GM = Cast<AExploreGameModeBase>(UGameplayStatics::GetGameMode(this)))
				{
					GM->RemoveLevelReqOnReload(this);
				}
				return;
			}
			else
			{
				Integrity = Info.ManagerHealth;
				float PercentIntegrity = Integrity / float(MaxIntegrity);
				if (HealthWidgetComponent)
				{
					HealthWidgetComponent->SetHealthPercent(PercentIntegrity);
				}
				ApprovedByGameMode();
			}
		}
	}
}

bool ARatManager::IsLevelRequirement()
{
	return true;
}

void ARatManager::HideManager()
{
	if (StrongholdMesh)
	{
		StrongholdMesh->SetVisibility(false);
		StrongholdMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (CollisionSphere) CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	BPHideManager();
}

void ARatManager::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser)
{
	if (bWasDestroyed) return;

	AddToIntegrity(-Damage);

	if (ABaseCat* Cat = Cast<ABaseCat>(DamageCauser->GetOwner()))
	{
		Cat->ReceiveAwardPoints(100.f);
	}

	if (Integrity <= 0.f)
	{
		InitiateDestruction();
		return;
	}

	float PercentIntegrity = Integrity / float(MaxIntegrity);

	if (HealthWidgetComponent)
	{
		HealthWidgetComponent->SetHealthPercent(PercentIntegrity);
	}
	ShouldShowHealthBar(true);

	if (GetWorldTimerManager().IsTimerActive(DamageTimer))
	{
		GetWorldTimerManager().ClearTimer(DamageTimer);
	}
	GetWorldTimerManager().SetTimer(DamageTimer, this, &ARatManager::DamageTimerFinished, DamageWaitTime);
	// TODO: Show damage-material or a VFX (like sparks) dependent on current integrity percent

}

void ARatManager::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor->ActorHasTag("Player"))
	{
		AlarmFX(true);

		// Call ALL available Rats to stronghold to protect it as cat is close by
		for (ARatCharacter* Rat : ManagedRats)
		{
			if (Rat->IsAvailbleToHelp() && Rat->IsUnoccupied())
			{
				OrderRatToTarget(Rat, OtherActor);
			}
			// NOTE: Must move to Cat/OtherActor's location, as it's easier for rats to find a nav-path towards. Also it makes more sense gameplay-wise.
		}
	}
}

void ARatManager::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor->ActorHasTag("Player"))
	{
		AlarmFX(false);
	}
}

void ARatManager::CancelOrders()
{
	if (RatsWithCurrentOrders.Num() > 0)
	{
		for (ARatCharacter* Rat : RatsWithCurrentOrders)
		{
			if (IsValid(Rat)) Rat->Reset();
		}
		RatsWithCurrentOrders.Empty();
	}
}

void ARatManager::InitiateDestruction()
{
	if (bWasDestroyed) return;

	bWasDestroyed = true;

	ShouldShowHealthBar(false);
	GetWorldTimerManager().ClearAllTimersForObject(this);
	CancelOrders();
	DumpRats();

	DestructionFX(); // BP side of 'destroying' this actor
	//Destroy(); // DO NOT actually destroy this actor. Need it to exist when we call Save

	// Alert Cat so it can show icon in HUD
	if (IPlayerInterface* Cat = Cast<IPlayerInterface>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		Cat->AddNewManager();
	}
	
	// Alert GameMode so it can check if level is complete
	if (AExploreGameModeBase* GM = Cast<AExploreGameModeBase>(UGameplayStatics::GetGameMode(this)))
	{
		GM->NewLevelReqDestroyed(this);
	}
}

void ARatManager::DumpRats()
{
	for (ARatUnit* Unit : UnitsAwaitingRespawn)
	{
		Unit->ShowAwaitingRespawn(false);
	}
	UnitsAwaitingRespawn.Empty();
	RatUnits.Empty();
	for (ARatCharacter* Rat : ManagedRats)
	{
		Rat->OnRatDeath.Clear();
		Rat->OnPlayerKilled.Clear();
		Rat->OnPlayerSeen.Clear();
		Rat->OnCriticalInjury.Clear();
		Rat->OnOrderCompleted.Clear();
	}
	ManagedRats.Empty();	
}

void ARatManager::ShouldShowHealthBar(bool bShow)
{
	if (HealthWidgetComponent)
	{
		HealthWidgetComponent->SetVisibility(bShow);
	}
}

void ARatManager::DamageTimerFinished()
{
	// TODO: Fade out/off damage-material
	ShouldShowHealthBar(false);
}

#pragma endregion Standard

#pragma region Manager
void ARatManager::SpawnAllRatsForUnit(ARatUnit* InUnit)
{
	if (!IsValid(InUnit)) return;

	// spawn one less than the number of patrol points per unit. IE if there are three patrol points, we should spawn 2 rats. Don't want every patrol point occupied because it just looks sort of silly in the game. 
	// BUT, always spawn at least one rat per unit, even if there's only one patrol point.

	for (int32 i = 0; i < InUnit->GetDesiredNumRats(); i++)
	{
		SpawnRatForUnit(InUnit);
	}
}

void ARatManager::SpawnRatForUnit(ARatUnit* InUnit)
{
	if (!IsValid(InUnit) || !IsValid(InUnit->GetRatClass())) return;
	if (InUnit->GetPatrolPoints().Num() < 1) return;

	int32 rand = FMath::RandRange(0, InUnit->GetPatrolPoints().Num() - 1);

	FTransform SpawnTransform = InUnit->GetSpawnTransform();
	FVector SpawnLocation = SpawnTransform.GetLocation();
	FRotator SpawnRotation = SpawnTransform.GetRotation().Rotator();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ARatCharacter* NewScout = GetWorld()->SpawnActor<ARatCharacter>(InUnit->GetRatClass(), SpawnLocation, SpawnRotation, SpawnParams);

	ManagedRats.Add(NewScout);

	NewScout->SetRatManager(this);
	NewScout->RatUnit = InUnit;
	NewScout->SetPatrolTarget(InUnit->GetPatrolPoints()[rand]);
	BindCallbacksToDelegates(NewScout);
}

void ARatManager::BindCallbacksToDelegates(ARatCharacter*& InRat)
{
	InRat->OnPlayerSeen.AddDynamic(this, &ARatManager::ReceivePlayerAlert);
	InRat->OnCriticalInjury.AddDynamic(this, &ARatManager::ReceiveCallForHelp);
	InRat->OnOrderCompleted.AddDynamic(this, &ARatManager::RatCompletedOrder);
	InRat->OnRatDeath.AddDynamic(this, &ARatManager::RemoveRat);
	InRat->OnPlayerKilled.AddDynamic(this, &ARatManager::ReceivePlayerDead);
}

void ARatManager::RemoveRat(ARatCharacter* InRat)
{
	UnitsAwaitingRespawn.Add(InRat->RatUnit);
	for (ARatUnit* Unit : UnitsAwaitingRespawn)
	{
		Unit->ShowAwaitingRespawn(true);
	}

	ManagedRats.Remove(InRat);
	RatsWithCurrentOrders.Remove(InRat);

	RatHasDiedFX();
}

void ARatManager::ReplaceDeadRats()
{
	if (bWasDestroyed) return;

	// Called via RatHasDiedFX() in blueprint (so we can have a delay between rat dying and respawning)
	// Call for one Unit at a time, since each rat death has a delay before respawning
	if (UnitsAwaitingRespawn.Num() > 0)
	{
		SpawnRatForUnit(UnitsAwaitingRespawn[0]);
		UnitsAwaitingRespawn[0]->ShowAwaitingRespawn(false);
		UnitsAwaitingRespawn.RemoveAt(0);
	}
}

FVector ARatManager::GetRandomPointInCollisionSphere()
{
	FVector Location = GetActorLocation();
	double Theta = FMath::FRand() * 2 * PI;
	double SpawnRadius = CollisionSphere->GetScaledSphereRadius() * DestructionSphereFactor;
	double Distance = SpawnRadius * FMath::Sqrt(FMath::FRand());
	double X = Location.X + Distance * FMath::Cos(Theta);
	double Y = Location.Y + Distance * FMath::Sin(Theta);

	return FVector(X, Y, Location.Z);
}

void ARatManager::GetAllRatsInRange(float InRange, FVector TargetLoc, TArray<ARatCharacter*>& OutRats)
{
	if (ManagedRats.Num() > 0.f)
	{
		for (ARatCharacter* Rat : ManagedRats)
		{
			if (!IsValid(Rat)) continue;
			float Distance = FVector::Distance(Rat->GetActorLocation(), TargetLoc);
			if (Distance < InRange)
			{
				OutRats.AddUnique(Rat);
			}
		}
	}
}

ARatCharacter* ARatManager::GetFirstRatInRange(float InRange, FVector TargetLoc)
{
	if (ManagedRats.Num() > 0.f)
	{
		for (ARatCharacter* Rat : ManagedRats)
		{
			if (!IsValid(Rat)) continue;
			float Distance = FVector::Distance(Rat->GetActorLocation(), TargetLoc);
			if (Distance < InRange)
			{
				return Rat;
			}
		}
	}
	return nullptr;
}

ARatCharacter* ARatManager::GetClosestRatToManager()
{
	if (ManagedRats.Num() > 0.f)
	{
		float NearestRatDistance = 1000000.f;
		ARatCharacter* NearestRat = nullptr;
		for (ARatCharacter* Rat : ManagedRats)
		{
			if (!IsValid(Rat)) continue;
			float Distance = FVector::Distance(Rat->GetActorLocation(), GetActorLocation());
			if (Distance < NearestRatDistance)
			{
				NearestRatDistance = Distance;
				NearestRat = Rat;
			}
		}
		return NearestRat;
	}
	return nullptr;
}

void ARatManager::OrderRatToLocation(ARatCharacter*& InRat, FVector& InLocation)
{
	InRat->ReceiveMoveToLocation(InLocation);
	RatsWithCurrentOrders.AddUnique(InRat);
	SignalRatFX(InRat->GetActorLocation());
}

void ARatManager::OrderRatToTarget(ARatCharacter* InRat, AActor* InTarget)
{
	InRat->ReceiveMoveToTarget(InTarget);
	RatsWithCurrentOrders.AddUnique(InRat);
	SignalRatFX(InRat->GetActorLocation());
}
#pragma endregion Manager

#pragma region AlertResponses
// These are the Manager's RESPONSES to Receiving an Alert from a Rat. As in, the manager relay's the rat's alert to all other rats.
void ARatManager::ReceivePlayerAlert(FVector PlayerLocation, ARatUnit* InRatUnit)
{
	if (bWasDestroyed) return;

	// Tell first available rat in Unit to go to Player location
	for (ARatCharacter* Rat : ManagedRats)
	{
		bool RatIsValid = Rat->RatUnit == InRatUnit &&
						Rat->IsUnoccupied() &&
						Rat->IsAvailbleToHelp();
		if (RatIsValid)
		{
			OrderRatToLocation(Rat, PlayerLocation);
			return;
		}
	}
}

void ARatManager::ReceiveCallForHelp(FVector RatLocation, ARatUnit* InRatUnit)
{
	if (bWasDestroyed) return;
	
	// Tell first available rat in Unit to go to Player location
	for (ARatCharacter* Rat : ManagedRats)
	{
		bool RatIsValid = Rat->RatUnit == InRatUnit &&
			Rat->IsUnoccupied() &&
			Rat->IsAvailbleToHelp();
		if (RatIsValid)
		{
			OrderRatToLocation(Rat, RatLocation);
			return;
		}
	}
}

void ARatManager::ReceivePlayerDead()
{
	if (bWasDestroyed) return;

	CancelOrders();
}

void ARatManager::RatCompletedOrder(ARatCharacter* InRat)
{
	if (bWasDestroyed) return;

	RatsWithCurrentOrders.Remove(InRat);
}

#pragma endregion AlertResponses
