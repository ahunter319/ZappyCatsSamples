// Copyright Alyssa Hunter 2024.

#pragma once

#include "CoreMinimal.h"
#include "CharacterEnums.h"
#include "AIController.h" 
#include "../Interface/CombatInterface.h"
#include "GameFramework/Character.h"
#include "RatCharacter.generated.h"

class AAIController;
class ARatManager;
class ARatUnit;
class UAnimMontage;
class UCombatComponent;
class UHealthComponent;
struct FAIRequestID;
struct FPathFollowingResult;
class UNiagaraSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLocationAlert, FVector, AlertLocation, ARatUnit*, RatUnit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRatAlert, ARatCharacter*, InRat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerKilled);

UCLASS()
class ZAPPYCATS_API ARatCharacter : public ACharacter, public ICombatInterface
{
	GENERATED_BODY()

public:
	ARatCharacter();

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaTime) override;

	void VisualizeBoundaries();

	void SetStateWidgetValues();
	void UpdatePlayerLastKnownLocation();

	// DAMAGE
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	void ReactToDamage(const FDamageEvent& DamageEvent);
	virtual void Destroyed() override;

	// Wrapper Movement functions called by Manager
	void ReceiveMoveToLocation(FVector& InLocation);
	void ReceiveMoveToTarget(AActor* InTarget);

	// Completely reset rat as if restarting game
	void Reset();

	// Tell rat to drop everything and receive new order
	void OverrideBehavior();

	// DELEGATES
	UPROPERTY(BlueprintAssignable)
	FOnLocationAlert OnPlayerSeen;

	UPROPERTY(BlueprintAssignable)
	FOnLocationAlert OnCriticalInjury;

	UPROPERTY(BlueprintAssignable)
	FOnRatAlert OnOrderCompleted;

	UPROPERTY(BlueprintAssignable)
	FOnRatAlert OnRatDeath;

	UPROPERTY(BlueprintAssignable)
	FOnPlayerKilled OnPlayerKilled;

	bool IsAvailbleToHelp();
	bool CanSeePlayer();

	// Use this to check if the rat has travelled more than AllowedTravelDistance from ANY of its Unit's patrol points or Manager. 
	bool IsWithinBounds();

	bool IsMoving();

	// Location where Rat will direct its attack
	UPROPERTY(VisibleAnywhere)
	FVector AttackLocation;

	UFUNCTION(BlueprintCallable)
	bool IsUnoccupied();

	void OnOrderedMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& PathFollowingResult);

#pragma region Properties
	// PROPERTIES

	// Distance from the Patrol point the Rat needs to be for a successful move
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float PatrolAcceptanceRadius = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float PatrolWaitMin = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float PatrolWaitMax = 6.f;

	// How close to a target the Rat needs to be in order to attack
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category= "Enemy")
	float AttackRadius = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float AttackWaitMin = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float AttackWaitMax = 2.f;

	// Minimum time between Search-cycles
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float SearchWaitMin = 2.f;

	// Maximum time between Search-cycles
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float SearchWaitMax = 6.f;

	// How long will we spend searching for hidden target before reseting
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float MaxResetTime = 7.f; 

	// Distance from each PatrolPoint or Manager that rat is allowed to travel
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float AllowedTravelDistance = 1500.f;

	// Multiplier for TravelDistance to use with determining range for Manager specifically. Since Manager's mesh is usually pretty big, AllowedTravelDistance might not be enough of a buffer. NOTE: This should probably be a property on the Manager, since we may use different meshes per level and they'll be different sizes. But leaving here for now. 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float ManagerDistanceMultiplier = 1.5f;

	// Use this to visualize the Rat's travel boundaries
	UPROPERTY(EditAnywhere, Category = "Enemy")
	bool bDebugBoundaries = false;

	// Distance Rat can "see"
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category= "Enemy")
	float SightRadius = 1500.f;

	// Rat will get this close to player before stopping movement
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float AcceptanceRadius = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float ChaseSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float PatrolSpeed = 100.f;

	// Points to award to player when Rat takes damage
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float RatBasePoints = 10.f;

#pragma endregion Properties

	// PATROLLING

	// Unit has access to manager, spawnpoint and patrol targets
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<ARatUnit> RatUnit; 

	// Current patrol target
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Enemy")
	AActor* PatrolTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Enemy")
	AActor* TargetPlayer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy")
	FVector LastKnownTargetLocation  = FVector(0.f, 0.f, 0.f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	FVector HitFromLocation = FVector(0.f, 0.f, 0.f);

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Enemy")
	FLinearColor EnemyLaserColor;

protected:
	virtual void BeginPlay() override;
	void BindCallbacksToDependencies();
	void DetermineNextEnemyAction();

	void AdjustWidgetScale();

	// Use to check if rat is within acceptance radius of its current patrol target
	bool IsAtTarget();

	// ANIMATIONS/MONTAGES
	void PlayFireAnimation();
	void PlayDeathAnimation();
	void PlayHitReactAnimation();
	void PlayShockAnimation();
	void PlaySearchAnimation();

	UFUNCTION()
	void HandleDeath();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void Dissolve();

	// STATE
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Enemy")
	EActionState ActionState = EActionState::EAS_Unoccupied;

	// Call via anim-notify when montage is finished to reset action state
	UFUNCTION(BlueprintCallable)
	void FinishAction();

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Enemy")
	EEnemyState EnemyState = EEnemyState::EES_Patrolling;

	// How much to scale Distance between Rat and Cat by before passing to widgets to adjust themselves accordingly.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = WidgetScale)
	float DistanceScalingFactor = 1000.f;

	// How far away can player still see the Rat's widgets
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = WidgetScale)
	float MaxWidgetViewDistance = 4000.f;

private:
	// ENEMY BEHAVIOR
	void InitializeEnemy();

	UFUNCTION(BlueprintCallable)
	void CheckSearchTarget();

	void CheckCombatTarget();
	void CheckPatrolTarget();

	void ShouldShowHealthBar(bool bShow);
	void ShouldShowStateWidget(bool bShow);

	UFUNCTION()
	void UpdateHealthBar();

	AActor* SelectNewPatrolTarget();

	bool InRange(AActor* InTarget, float Range);
	bool CanSmellPlayer();

	void ChaseTarget();
	void MoveToPatrol();
	void MoveToTarget(AActor* InTarget);
	void MoveToLocation(FVector& InLocation);
	void MoveWithinLastKnownRadius();

	void TurnTowardsHitDirection(FVector InLocation);
	void TurnInRandomDirection();

	bool CanAttack();
	void BeginAttack();

	UFUNCTION(BlueprintCallable)
	void Attack();

	UFUNCTION(BlueprintCallable)
	void AttackEnd();

	UFUNCTION(BlueprintCallable)
	void ReactEnd();

	void LoseInterest();

	// TIMERS
	FTimerHandle PatrolTimer;
	void ClearPatrolTimer();
	void PatrolTimerFinished();

	FTimerHandle AttackTimer;
	void StartAttackTimer();
	void ClearAttackTimer();

	FTimerHandle SearchTimer;
	void SearchTimerFinished();
	void ClearSearchTimer();

	FTimerHandle ResetTimer;
	void StartResetTimer();
	void ClearResetTimer();

	FTimerHandle HitWaitTimer;
	void ClearHitWaitTimer();

	FTimerHandle CheckMovementTimer;
	void CheckMovementTimerFinished();
	void ClearCheckMovementTimer();

	// SENSING
	UFUNCTION()
	void PawnSeen(APawn* SeenPawn); 

	UPROPERTY(VisibleAnywhere)
	class UPawnSensingComponent* PawnSensing;

	// CUSTOM COMPONENTS
	UPROPERTY()
	AAIController* EnemyController;

	// Keep for listening to delegates?
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<ARatManager> RatManager;
	
	// WIDGETS
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UHealthWidgetComponent* HealthWidgetComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UStateWidgetComponent* StateWidgetComponent;

	// COMPONENTS
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCombatComponent> CombatComp;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UHealthComponent> HealthComp;

	// ANIMATION
	UPROPERTY(EditAnywhere, Category = Montage)
	UAnimMontage* ShootMontage;

	// ANIMATION/MONTAGES FOR COMBAT INTERFACE
	UPROPERTY(EditAnywhere, Category = Montage)
	UAnimMontage* HitReactMontage;

	UPROPERTY(EditAnywhere, Category = Montage)
	UAnimMontage* ShockMontage;

	UPROPERTY(EditAnywhere, Category = Montage)
	UAnimMontage* DeathMontage;

	UPROPERTY(EditAnywhere, Category = Montage)
	UAnimMontage* SearchMontage;

	// VFX
	UPROPERTY(EditAnywhere, Category = "VFX")
	TObjectPtr<UNiagaraSystem> ShockVFX;

public:
	FORCEINLINE bool IsSearching() { return EnemyState == EEnemyState::EES_Searching; }
	FORCEINLINE bool IsPatrolling() { return EnemyState == EEnemyState::EES_Patrolling; }
	FORCEINLINE bool IsAttacking() { return EnemyState == EEnemyState::EES_Attacking; }
	FORCEINLINE bool IsChasing() { return EnemyState == EEnemyState::EES_Chasing; }
	FORCEINLINE bool IsEngaged() { return EnemyState == EEnemyState::EES_Engaged; }
	FORCEINLINE bool IsFollowingOrder() { return EnemyState == EEnemyState::EES_FollowingOrder; }

	FORCEINLINE UHealthComponent* GetHealthComponent() { return HealthComp; }

	FORCEINLINE EEnemyState GetEnemyState() { return EnemyState; }
	FORCEINLINE EActionState GetActionState() { return ActionState; }

	FORCEINLINE void SetRatManager(ARatManager* InManager) { RatManager = InManager; }
	FORCEINLINE void SetRatUnit(ARatUnit* InUnit) { RatUnit = InUnit; }
	FORCEINLINE void SetPatrolTarget(AActor* InPoint) { PatrolTarget = InPoint; }
};