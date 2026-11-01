// Copyright Alyssa Hunter 2024.


#include "RatCharacter.h"
#include "BaseCat.h" // REMOVE?
#include "RatManager.h"
#include "../Interface/PlayerInterface.h"
#include "../Actors/RatUnit.h"
#include "../Components/CombatComponent.h"
#include "../Components/HealthComponent.h"
#include "../HUD/HealthWidgetComponent.h"
#include "../HUD/StateWidgetComponent.h"
#include "../Damage/DamageType_Melee.h"
#include "../Game/ExploreGameModeBase.h"

#include "Engine/DamageEvents.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/PawnSensingComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "AITypes.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"


#pragma region Standard
ARatCharacter::ARatCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);

	GetCharacterMovement()->bOrientRotationToMovement = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->MaxFlySpeed = PatrolSpeed;
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;

	HealthWidgetComponent = CreateDefaultSubobject<UHealthWidgetComponent>(TEXT("Health Bar"));
	HealthWidgetComponent->SetupAttachment(GetRootComponent());

	StateWidgetComponent = CreateDefaultSubobject<UStateWidgetComponent>(TEXT("State Box"));
	StateWidgetComponent->SetupAttachment(GetRootComponent());
	StateWidgetComponent->SetVisibility(true);

	PawnSensing = CreateDefaultSubobject<UPawnSensingComponent>(TEXT("Pawn Sensing"));
	PawnSensing->SightRadius = SightRadius;
	PawnSensing->SetPeripheralVisionAngle(90.f);

	CombatComp = CreateDefaultSubobject<UCombatComponent>(TEXT("Combat Component"));

	HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("Health Component"));

	Tags.Add(FName("LaserTarget"));
	Tags.Add(FName("Shockable"));
}

void ARatCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (IsValid(CombatComp))
	{
		CombatComp->SetCharacter(this);
	}
	if (IsValid(HealthComp))
	{
		HealthComp->SetCharacter(this);
	}
}

void ARatCharacter::BeginPlay()
{
	Super::BeginPlay();

	BindCallbacksToDependencies();
	InitializeEnemy();
}

void ARatCharacter::BindCallbacksToDependencies()
{
	if (HealthComp)
	{
		HealthComp->OnHealthChanged.AddDynamic(this, &ARatCharacter::UpdateHealthBar);
		HealthComp->OnCharacterDeath.AddDynamic(this, &ARatCharacter::HandleDeath);
	}

	if (PawnSensing)
	{
		PawnSensing->OnSeePawn.AddDynamic(this, &ARatCharacter::PawnSeen);
	}
}

void ARatCharacter::InitializeEnemy()
{
	SpawnDefaultController();
	EnemyController = Cast<AAIController>(GetController());

	ShouldShowHealthBar(false);
	ShouldShowStateWidget(false);
	if (HealthComp) HealthComp->ResetHealthToMax();
	UpdateHealthBar();

	MoveToPatrol();
}

void ARatCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HealthComp && HealthComp->IsDead()) return;
	if (!IsUnoccupied()) return;

	// Update Widgets
	AdjustWidgetScale();
	SetStateWidgetValues();

	UpdatePlayerLastKnownLocation();

	DetermineNextEnemyAction();

	// Visualize the Rat's travel boundaries
	if (bDebugBoundaries)
	{
		VisualizeBoundaries();
	}
}

void ARatCharacter::VisualizeBoundaries()
{
	if (PatrolTarget)
	{
		DrawDebugSphere(GetWorld(), PatrolTarget->GetActorLocation(), AllowedTravelDistance, 12, FColor::Cyan);
	}
	if (RatUnit && RatUnit->GetManager())
	{
		DrawDebugSphere(GetWorld(), RatUnit->GetManager()->GetActorLocation(), AllowedTravelDistance * ManagerDistanceMultiplier, 12, FColor::Cyan);
	}
	if (RatUnit && RatUnit->GetPatrolPoints().Num() > 1)
	{
		for (AActor* Point : RatUnit->GetPatrolPoints())
		{
			if (Point != PatrolTarget)
			{
				DrawDebugSphere(GetWorld(), Point->GetActorLocation(), AllowedTravelDistance, 12, FColor::Cyan);
			}
		}
	}
}

void ARatCharacter::UpdatePlayerLastKnownLocation()
{
	// Update Player's last known location
	if (IsValid(TargetPlayer) && CanSeePlayer())
	{
		LastKnownTargetLocation = TargetPlayer->GetActorLocation();
	}
}

void ARatCharacter::DetermineNextEnemyAction()
{
	// Don't interrupt an order
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

void ARatCharacter::AdjustWidgetScale()
{
	if (!IsValid(HealthWidgetComponent) || !IsValid(StateWidgetComponent)) return;

	// Get the distance between this rat and the Player's camera
	FVector CameraLoc = GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetCameraLocation();
	float Distance = FVector::Distance(GetActorLocation(), CameraLoc);

	if (Distance > MaxWidgetViewDistance)
	{
		ShouldShowHealthBar(false);
		ShouldShowStateWidget(false);
	}
	else
	{
		// Only show healthbar if rat has taken damage
		ShouldShowStateWidget(true);
		float DistanceScaled = Distance / DistanceScalingFactor;
		StateWidgetComponent->ScaleWidgetByFactor(DistanceScaled);
		HealthWidgetComponent->ScaleWidgetByFactor(DistanceScaled);
	}
}

float ARatCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (IsValid(HealthComp) && HealthComp->IsAlive()) HealthComp->ApplyDamage(DamageAmount);

	// Award points to Player and Get Cat's Laser Color for VFX
	if (!EventInstigator) return 0.0f;
	if (ABaseCat* OwningCat = Cast<ABaseCat>(EventInstigator->GetPawn()))
	{
		EnemyLaserColor = OwningCat->LaserColor;
		OwningCat->ReceiveAwardPoints(RatBasePoints);
	}

	// Early return if rat has died
	if (!IsValid(HealthComp) || !HealthComp->IsAlive()) return 0.0f;
	
	ReactToDamage(DamageEvent);
	ShouldShowHealthBar(true);

	ClearPatrolTimer();
	ClearHitWaitTimer();
	ClearSearchTimer();

	// Store the hit-from direction. Gives chance to see attacker. We'll turn after hit-react montage plays and calls ReactEnd().
	if (IsValid(EventInstigator) && EventInstigator->GetPawn()->ActorHasTag(FName("Player")))
	{
		HitFromLocation = EventInstigator->GetPawn()->GetActorLocation();
	}

	if (HealthComp && HealthComp->IsCriticallyHurt())
	{
		// Call for help from nearby rats if available
		OnCriticalInjury.Broadcast(GetActorLocation(), RatUnit);
	}
	return 0.0f;
}

void ARatCharacter::ReactToDamage(const FDamageEvent& DamageEvent)
{
	// Determine damage type and play appropriate animation
	if (IsValid(EnemyController)) EnemyController->StopMovement();

	if (UDamageType_Melee* CustomDamageType = Cast<UDamageType_Melee>(DamageEvent.DamageTypeClass->GetDefaultObject()))
	{
		if (CustomDamageType->SCDamageType == ESCDamageType::ESCDT_Electrical)
		{
			PlayShockAnimation();
			if (ShockVFX)
			{
				UNiagaraComponent* ShockComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ShockVFX, GetActorLocation());
				ShockComp->SetVariableLinearColor(FName("ShockColor"), EnemyLaserColor);
			}
		}
		else
		{
			PlayHitReactAnimation();
		}
	}
}

void ARatCharacter::ShouldShowHealthBar(bool bShow)
{
	if (HealthWidgetComponent)
	{
		HealthWidgetComponent->SetVisibility(bShow);
	}
}

void ARatCharacter::ShouldShowStateWidget(bool bShow)
{
	if (StateWidgetComponent)
	{
		StateWidgetComponent->SetVisibility(bShow);
	}
}

void ARatCharacter::UpdateHealthBar()
{
	if (HealthWidgetComponent && HealthComp)
	{
		HealthWidgetComponent->SetHealthPercent(HealthComp->GetPercentHealth());
	}
}

void ARatCharacter::SetStateWidgetValues()
{
	if (StateWidgetComponent)
	{
		StateWidgetComponent->SetVisualBasedOnState(EnemyState);
	}
}

void ARatCharacter::HandleDeath()
{
	// called via HealthComp
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->StopMovementImmediately();
	Tags.Add(FName("Dead"));

	ShouldShowHealthBar(false);
	ShouldShowStateWidget(false);

	GetWorldTimerManager().ClearAllTimersForObject(this);

	PlayDeathAnimation(); // This calls Destroy at the end

	OnRatDeath.Broadcast(this);
}

void ARatCharacter::Destroyed()
{
	Super::Destroyed();
}

#pragma endregion Standard

#pragma region EnemyBehavior
void ARatCharacter::CheckSearchTarget()
{
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
	MoveWithinLastKnownRadius();
}

void ARatCharacter::CheckCombatTarget()
{
	if (!IsValid(TargetPlayer))
	{
		Reset();
		return;
	}

	// Player is hiding, but if we can smell them it wont work
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

void ARatCharacter::CheckPatrolTarget()
{
	if (GetWorldTimerManager().IsTimerActive(PatrolTimer)) return;

	const float WaitTime = FMath::RandRange(PatrolWaitMin, PatrolWaitMax);
	GetWorldTimerManager().SetTimer(PatrolTimer, this, &ARatCharacter::PatrolTimerFinished, WaitTime);
}

AActor* ARatCharacter::SelectNewPatrolTarget()
{
	if (!RatUnit || !(RatUnit->GetPatrolPoints().Num() > 0)) return PatrolTarget;

	TArray<AActor*> ValidTargets;
	for (AActor* Target : RatUnit->GetPatrolPoints())
	{
		if (Target != PatrolTarget)
		{
			ValidTargets.AddUnique(Target);
		}
	}

	const int32 NumPatrolTargets = ValidTargets.Num();
	if (NumPatrolTargets > 0)
	{
		const int32 NewIndex = FMath::RandRange(0, NumPatrolTargets - 1);
		return ValidTargets[NewIndex];
	}

	return PatrolTarget;
}

void ARatCharacter::PawnSeen(APawn* SeenPawn)
{
	if (IsValid(EnemyController)) EnemyController->StopMovement();

	const bool bShouldEngage =
		HealthComp &&
		HealthComp->IsAlive() &&
		IsUnoccupied() &&
		EnemyState < EEnemyState::EES_Attacking &&
		SeenPawn->ActorHasTag("Player");

	const bool bTargetIsDead = SeenPawn->ActorHasTag("Dead");
	
	if (bShouldEngage && !bTargetIsDead)
	{
		TargetPlayer = SeenPawn;
		LastKnownTargetLocation = TargetPlayer->GetActorLocation();
		ClearPatrolTimer();
		ClearSearchTimer();
		ClearResetTimer();
		ClearHitWaitTimer();
		ChaseTarget();

		OnPlayerSeen.Broadcast(SeenPawn->GetActorLocation(), RatUnit);
	}
}

void ARatCharacter::ChaseTarget()
{
	EnemyState = EEnemyState::EES_Chasing;
	GetCharacterMovement()->MaxFlySpeed = ChaseSpeed;
	GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;

	MoveToTarget(TargetPlayer);
}

void ARatCharacter::MoveToPatrol()
{
	if (!IsValid(EnemyController)) return;

	ClearCheckMovementTimer();

	EnemyState = EEnemyState::EES_Patrolling;
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
	GetCharacterMovement()->MaxFlySpeed = PatrolSpeed;

	if (!IsValid(PatrolTarget)) PatrolTarget = SelectNewPatrolTarget();

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalActor(PatrolTarget);
	MoveRequest.SetAcceptanceRadius(PatrolAcceptanceRadius);
	EnemyController->MoveTo(MoveRequest);
}

void ARatCharacter::MoveToTarget(AActor* InTarget)
{
	if (!IsValid(EnemyController) || !IsValid(InTarget)) return;

	ClearCheckMovementTimer();

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance) AnimInstance->StopAllMontages(0.1f);

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalActor(InTarget);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
	EnemyController->MoveTo(MoveRequest);
}

void ARatCharacter::MoveToLocation(FVector& InLocation)
{
	if (!IsValid(EnemyController)) return;

	ClearCheckMovementTimer();

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance) AnimInstance->StopAllMontages(0.1f);

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(InLocation);
	MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
	EnemyController->MoveTo(MoveRequest);
}

void ARatCharacter::ReceiveMoveToLocation(FVector& InLocation)
{
	// Movement ordered by RatManager/Stronghold
	if (!IsValid(EnemyController)) return;

	OverrideBehavior();
	GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;

	EnemyController->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
	EnemyController->GetPathFollowingComponent()->OnRequestFinished.AddUObject(this, &ARatCharacter::OnOrderedMoveFinished);

	MoveToLocation(InLocation);
}

void ARatCharacter::ReceiveMoveToTarget(AActor* InTarget)
{
	// Movement ordered by RatManager/Stronghold
	if (!IsValid(EnemyController)) return;

	OverrideBehavior();
	GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;

	EnemyController->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
	EnemyController->GetPathFollowingComponent()->OnRequestFinished.AddUObject(this, &ARatCharacter::OnOrderedMoveFinished);

	MoveToTarget(InTarget);
}

void ARatCharacter::OnOrderedMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& PathFollowingResult)
{
	OnOrderCompleted.Broadcast(this);

	EnemyState = EEnemyState::EES_Patrolling;
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;

	if (!IsValid(EnemyController)) return;
	EnemyController->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
}

void ARatCharacter::MoveWithinLastKnownRadius()
{
	double Theta = FMath::FRand() * 2 * PI;
	double SearchRadius = 500.f;
	double Distance = SearchRadius * FMath::Sqrt(FMath::FRand());
	double X = LastKnownTargetLocation.X + Distance * FMath::Cos(Theta);
	double Y = LastKnownTargetLocation.Y + Distance * FMath::Sin(Theta);
	FVector NewLocation = FVector(X, Y, GetActorLocation().Z);
	MoveToLocation(NewLocation);

	// from https://www.mathworks.com/matlabcentral/answers/72915-creating-random-points-in-a-circle
	/*t = 2 * pi * rand(n, 1);
	r = R * sqrt(rand(n, 1));
	x = x0 + r.*cos(t);
	y = y0 + r.*sin(t);
	plot(x, y)*/
}

void ARatCharacter::TurnTowardsHitDirection(FVector InLocation)
{
	float DeltaSeconds = UGameplayStatics::GetWorldDeltaSeconds(this);

	FRotator LookAt = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), InLocation);
	FRotator Forward = GetActorForwardVector().Rotation();
	FRotator NewRot = UKismetMathLibrary::RInterpTo(Forward, LookAt, DeltaSeconds, 200.f);
	NewRot.Roll = 0.f;
	NewRot.Pitch = 0.f;

	SetActorRotation(NewRot.Quaternion());
}

void ARatCharacter::TurnInRandomDirection()
{
	//rotate enemy in random direction/yaw
	FRotator RandomYaw = UKismetMathLibrary::MakeRotator(0.f, 0.f, FMath::RandRange(1.f, 360.f));
	FRotator Forward = UKismetMathLibrary::MakeRotFromX(GetActorForwardVector());
	Forward.Pitch = 0.f;
	Forward.Roll = 0.f;
	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(Forward, RandomYaw);
	float DeltaSeconds = UGameplayStatics::GetWorldDeltaSeconds(this);
	AddActorWorldRotation(DeltaRot * DeltaSeconds * 10.f);
}

void ARatCharacter::BeginAttack()
{
	if (!IsValid(TargetPlayer) || !IsValid(CombatComp)) return;

	EnemyState = EEnemyState::EES_Engaged;
	AttackLocation = TargetPlayer->GetActorLocation();
	
	Execute_UpdateCombatFacingTarget(this, AttackLocation);
	PlayFireAnimation();
}

void ARatCharacter::Attack()
{
	// Called from AnimBP via AnimNotify_Fire in ShootMontage
	if (!IsValid(TargetPlayer) || !IsValid(CombatComp)) return;

	CombatComp->SpawnProjectile(AttackLocation);
}

void ARatCharacter::AttackEnd()
{
	EnemyState = EEnemyState::EES_Default;
	CheckCombatTarget();
}

void ARatCharacter::ReactEnd()
{
	TurnTowardsHitDirection(HitFromLocation);

	// Only want to look around then reset if we haven't seen the player yet
	if (EnemyState < EEnemyState::EES_Chasing)
	{
		GetWorldTimerManager().SetTimer(HitWaitTimer, this, &ARatCharacter::Reset, 5.f);
	}

	// In case hitreact interrupted an attack, and therefore Rat is Engaged, need to set back to attack
	else if (EnemyState >= EEnemyState::EES_Attacking)
	{
		EnemyState = EEnemyState::EES_Chasing;
		CheckCombatTarget();
	}
}

void ARatCharacter::LoseInterest()
{
	TargetPlayer = nullptr;
	EnemyState = EEnemyState::EES_Default;
	ShouldShowHealthBar(false);
}

void ARatCharacter::Reset()
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	LoseInterest();
	MoveToPatrol();
}

void ARatCharacter::OverrideBehavior()
{
	EnemyState = EEnemyState::EES_FollowingOrder;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance) AnimInstance->StopAllMontages(0.1f);

	if (IsValid(EnemyController)) EnemyController->StopMovement();

	GetWorldTimerManager().ClearAllTimersForObject(this);
}

#pragma endregion EnemyBehavior

#pragma region Helpers
bool ARatCharacter::InRange(AActor* InTarget, float Range)
{
	if (!IsValid(InTarget)) return false;

	float Distance = (InTarget->GetActorLocation() - GetActorLocation()).Size();
	return Distance <= Range;
}

bool ARatCharacter::IsWithinBounds()
{
	bool bInRangeOfPatrolTarget = InRange(PatrolTarget, AllowedTravelDistance);

	bool bInRangeOfAnyPatrolInUnit = false;

	for (AActor* Target : RatUnit->GetPatrolPoints())
	{
		if (InRange(Target, AllowedTravelDistance))
		{
			bInRangeOfAnyPatrolInUnit = true;
			break;
		}
	}

	bool bInRangeOfManager = InRange(RatUnit->GetManager(), AllowedTravelDistance * ManagerDistanceMultiplier);

	return bInRangeOfPatrolTarget || bInRangeOfAnyPatrolInUnit || bInRangeOfManager;
}

bool ARatCharacter::IsMoving()
{
	return GetCharacterMovement()->GetLastUpdateVelocity().Length() > 1.f;
}

bool ARatCharacter::IsAtTarget()
{
	if (!IsValid(PatrolTarget)) return false;

	/*DrawDebugSphere(GetWorld(), PatrolTarget->GetActorLocation(), PatrolAcceptanceRadius + 100.f, 12, FColor::Red, true, 10.f);*/
	// + 100.f for some margin of error
	return FVector::Distance(GetActorLocation(), PatrolTarget->GetActorLocation()) < PatrolAcceptanceRadius + 100.f;
}

bool ARatCharacter::CanSeePlayer()
{
	if (!IsValid(EnemyController) || !IsValid(TargetPlayer)) return false;

	FVector EyePosition = GetMesh()->GetSocketLocation(FName("EyeSocket"));
	return EnemyController->LineOfSightTo(TargetPlayer, EyePosition);
}

bool ARatCharacter::CanSmellPlayer()
{
	if (!IsValid(TargetPlayer)) return false;

	if (ABaseCat* Cat = Cast<ABaseCat>(TargetPlayer))
	{
		return Cat->IsCatSmelly();
	}
	return false;
}

bool ARatCharacter::CanAttack()
{
	bool bTargetAlive = IsValid(TargetPlayer) && !TargetPlayer->ActorHasTag("Dead");

	bool bCanAttack =
		InRange(TargetPlayer, AttackRadius) &&
		!IsAttacking() &&
		!IsEngaged() &&
		HealthComp &&
		HealthComp->IsAlive() &&
		IsUnoccupied();

	return bTargetAlive && bCanAttack;
}

bool ARatCharacter::IsAvailbleToHelp()
{
	return EnemyState < EEnemyState::EES_Chasing;
}

bool ARatCharacter::IsUnoccupied()
{
	return ActionState == EActionState::EAS_Unoccupied;
}

void ARatCharacter::FinishAction()
{
	ActionState = EActionState::EAS_Unoccupied;
}
#pragma endregion Helpers

#pragma region Timer
void ARatCharacter::PatrolTimerFinished()
{
	PatrolTarget = SelectNewPatrolTarget();
	MoveToPatrol();
}

void ARatCharacter::ClearPatrolTimer()
{
	GetWorldTimerManager().ClearTimer(PatrolTimer);
}

void ARatCharacter::StartAttackTimer()
{
	EnemyState = EEnemyState::EES_Attacking;
	const float AttackTime = FMath::RandRange(AttackWaitMin, AttackWaitMax);
	GetWorldTimerManager().SetTimer(AttackTimer, this, &ARatCharacter::BeginAttack, AttackTime);
}

void ARatCharacter::ClearAttackTimer()
{
	GetWorldTimerManager().ClearTimer(AttackTimer);
}

void ARatCharacter::SearchTimerFinished()
{
	MoveWithinLastKnownRadius();
}

void ARatCharacter::ClearSearchTimer()
{
	GetWorldTimerManager().ClearTimer(SearchTimer);
}

void ARatCharacter::StartResetTimer()
{
	EnemyState = EEnemyState::EES_Searching;
	GetWorldTimerManager().SetTimer(ResetTimer, this, &ARatCharacter::Reset, MaxResetTime);
}

void ARatCharacter::ClearResetTimer()
{
	GetWorldTimerManager().ClearTimer(ResetTimer);
}

void ARatCharacter::ClearHitWaitTimer()
{
	GetWorldTimerManager().ClearTimer(HitWaitTimer);
}

void ARatCharacter::CheckMovementTimerFinished()
{
	if (!IsMoving() && IsValid(TargetPlayer))
	{
		FinishAction();
		if (CanSeePlayer()) ChaseTarget();
		else MoveToPatrol();
	}
}

void ARatCharacter::ClearCheckMovementTimer()
{
	GetWorldTimerManager().ClearTimer(CheckMovementTimer);
}

#pragma endregion Timer

#pragma region Animation
void ARatCharacter::PlayFireAnimation()
{
	ActionState = EActionState::EAS_Attacking;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ShootMontage)
	{
		AnimInstance->Montage_Play(ShootMontage);
		FName SectionName("Default");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ARatCharacter::PlayHitReactAnimation()
{
	ActionState = EActionState::EAS_Reacting;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HitReactMontage)
	{
		AnimInstance->Montage_Play(HitReactMontage);
		FName SectionName("Default");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ARatCharacter::PlayDeathAnimation()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && DeathMontage)
	{
		AnimInstance->Montage_Play(DeathMontage);
		FName SectionName("Default");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ARatCharacter::PlayShockAnimation()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ShockMontage)
	{
		AnimInstance->Montage_Play(ShockMontage);
		FName SectionName("Default");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ARatCharacter::PlaySearchAnimation()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && SearchMontage)
	{
		AnimInstance->Montage_Play(SearchMontage);
		FName SectionName("Default");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

#pragma endregion Animation

