// Copyright Alyssa Hunter 2025.


#include "AutoShooter.h"
#include "../HUD/HealthWidgetComponent.h"
#include "Projectile.h"
#include "../Interface/SaveGameInterface.h"
#include "../Library/CommonFunctionLibrary.h"

#include "Components/SphereComponent.h"
#include "Perception/PawnSensingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"


AAutoShooter::AAutoShooter()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(CollisionSphere);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);

	MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle Location"));
	MuzzleLocation->SetupAttachment(Mesh);

	PawnSensing = CreateDefaultSubobject<UPawnSensingComponent>(TEXT("Pawn Sensing"));
	PawnSensing->SightRadius = AttackRange;
	PawnSensing->SetPeripheralVisionAngle(45.f);
	PawnSensing->bSeePawns = true;
	PawnSensing->bHearNoises = false;

	HealthWidgetComponent = CreateDefaultSubobject<UHealthWidgetComponent>(TEXT("Health Bar"));
	HealthWidgetComponent->SetupAttachment(GetRootComponent());

	Tags.Add(FName("LaserTarget"));
	Tags.Add(FName("Destructible"));

	DestructibleTag = UCommonFunctionLibrary::GetUniqueNameWithParam(FString("Destructible"));
}

void AAutoShooter::BeginPlay()
{
	Super::BeginPlay();
	
	OnTakeAnyDamage.AddDynamic(this, &AAutoShooter::ReceiveDamage);

	if (PawnSensing) PawnSensing->OnSeePawn.AddDynamic(this, &AAutoShooter::PawnSeen);

	Health = MaxHealth;
	HealthWidgetComponent->SetHealthPercent(Health / MaxHealth);
	ShouldShowHealthBar(false);
}

void AAutoShooter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AAutoShooter::SaveData(UZappyCats_SaveGame& OutSaveGame)
{
	for (FManagerInfo& Info : OutSaveGame.GameProps.DamageableActors)
	{
		if (Info.ManagerName == DestructibleTag)
		{
			Info.ManagerHealth = Health;
			return;
		}
	}
	FManagerInfo NewInfo = FManagerInfo();
	NewInfo.ManagerHealth = Health;
	NewInfo.ManagerName = DestructibleTag;
	OutSaveGame.GameProps.DamageableActors.Add(NewInfo);
}

void AAutoShooter::LoadData(UZappyCats_SaveGame* SaveGameRef)
{
	if (!SaveGameRef) return;

	for (FManagerInfo Info : SaveGameRef->GameProps.DamageableActors)
	{
		if (Info.ManagerName == DestructibleTag)
		{
			if (Info.ManagerHealth <= 0.f)
			{
				Health = 0.f;
				Hide();
				return;
			}
			else
			{
				Health = Info.ManagerHealth;
				float PercentHealth = Health / float(MaxHealth);
				if (HealthWidgetComponent)
				{
					HealthWidgetComponent->SetHealthPercent(PercentHealth);
				}
			}
		}
	}
}

bool AAutoShooter::IsLevelRequirement()
{
	return false;
}

void AAutoShooter::Hide()
{
	if (Mesh)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (PawnSensing)
	{
		PawnSensing->bSeePawns = false;
	}
	if (CollisionSphere) CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAutoShooter::ShouldShowHealthBar(bool bShow)
{
	if (IsValid(HealthWidgetComponent))
	{
		HealthWidgetComponent->SetVisibility(bShow);
	}
}

void AAutoShooter::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, class AController* InstigatorController, AActor* DamageCauser)
{
	if (Health <= 0.f) return;

	AddToHealth(-Damage);

	ShouldShowHealthBar(true);
	if (HealthWidgetComponent)
	{
		HealthWidgetComponent->SetHealthPercent(GetPercentHealth());
	}

	if (Health <= 0.f)
	{
		GetWorldTimerManager().ClearAllTimersForObject(this); // ensures we don't fire while being destroyed or after being destroyed (yes this was happening)
		Hide();
		DestructionFX(); // this will hide everything in BP, to appear as though it's been destroyed
	}
}

void AAutoShooter::PawnSeen(APawn* SeenPawn)
{
	if (Target) return; // don't override already seen pawn
	if (Health <= 0.f) return;

	if (SeenPawn->ActorHasTag("Player") && !SeenPawn->ActorHasTag("Dead"))
	{
		Target = SeenPawn;
	}

	if (GetWorldTimerManager().IsTimerActive(AttackTimer)) return;
	if (IsValid(Target)) WaitForNextAttack();
}

void AAutoShooter::WaitForNextAttack()
{
	float FireRate = FMath::RandRange(FireRateMin, FireRateMax);
	PrepareToFireFX();
	GetWorldTimerManager().SetTimer(AttackTimer, this, &AAutoShooter::Attack, FireRate);
}

bool AAutoShooter::TargetInRange()
{
	if (!IsValid(Target)) return false;

	if (FVector::Distance(GetActorLocation(), Target->GetActorLocation()) < AttackRange)
	{
		return true;
	}
	return false;
}

bool AAutoShooter::HasLineOfSight()
{
	if (!IsValid(GetController()) || !IsValid(Target)) return false;

	return GetController()->LineOfSightTo(Target, MuzzleLocation->GetComponentLocation());
}

bool AAutoShooter::TargetWithinArch()
{
	if (!IsValid(Target)) return false;

	FVector Forward = MuzzleLocation->GetComponentLocation() - GetActorLocation();
	FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	
	//FRotator Delta = UKismetMathLibrary::FindLookAtRotation(Forward, ToTarget);
	FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(Forward.Rotation(), ToTarget.Rotation());

	bool WithinYaw = false;
	bool WithinPitch = false;
	bool WithinRoll = false;

	if (Delta.Yaw < LookAtArchLimit && Delta.Yaw > -LookAtArchLimit)
	{
		WithinYaw = true;
	}
	if (Delta.Pitch < LookAtArchLimit && Delta.Pitch > -LookAtArchLimit)
	{
		WithinPitch = true;
	}
	if (Delta.Roll < LookAtArchLimit && Delta.Roll > -LookAtArchLimit)
	{
		WithinRoll = true;
	}
	return WithinYaw && WithinPitch && WithinRoll;
}

void AAutoShooter::Attack()
{
	bool bCanFire = IsValid(ProjectileClass) && TargetInRange() && TargetWithinArch() && HasLineOfSight();

	if (!bCanFire)
	{
		Target = nullptr;
		ShouldShowHealthBar(false);
		CancelFX();
		return;
	}

	FireFX();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	FVector Start = MuzzleLocation->GetComponentLocation();
	FVector TargetLocation = Target->GetActorLocation() - Start;
	FRotator TargetRotation = TargetLocation.Rotation();

	AProjectile* SpawnedProjectile = GetWorld()->SpawnActor<AProjectile>(ProjectileClass, Start, TargetRotation, SpawnParams);

	if (IsValid(Target) && !Target->ActorHasTag(FName("Dead"))) WaitForNextAttack();
}


