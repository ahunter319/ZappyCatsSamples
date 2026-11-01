// Copyright Alyssa Hunter 2024.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Interface/LevelRequirementInterface.h"
#include "../Interface/SaveableInterface.h"
#include "RatManager.generated.h"

class ARatUnit;
class ARatCharacter;
class UHealthWidgetComponent;
class USphereComponent;
class UNiagaraComponent;
class UZappyCats_SaveGame;

// Dispatcher is probably a better name. Manages communication between rats in their patrolgroups/units, handles spawning/respawning of rats

UCLASS()
class ZAPPYCATS_API ARatManager : public AActor, public ISaveableInterface
{
	GENERATED_BODY()
	
public:	
	ARatManager();
	virtual void Tick(float DeltaTime) override;

	FName GetNameTag();
	void ApprovedByGameMode();

	// SAVEABLE INTERFACE
	virtual void SaveData(UZappyCats_SaveGame& OutSaveGame) override;
	virtual void LoadData(UZappyCats_SaveGame* SaveGameRef) override;
	virtual bool IsLevelRequirement() override;

	void HideManager();

	// Hide Manager at BP level (any added VFX etc)
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void BPHideManager();

	UFUNCTION()
	void ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, class AController* InstigatorController, AActor* DamageCauser);

	// Listen for delegates broadcasted by the InRat
	void BindCallbacksToDelegates(ARatCharacter*& InRat);

	// RATUNITS
	void HandleSpawningRats(); // wrapper to be called by game mode
	void SpawnAllRatsForUnit(ARatUnit* InUnit);
	void SpawnRatForUnit(ARatUnit* InUnit);

	// Use to get random location within radius for spawning VFX
	UFUNCTION(BlueprintCallable)
	FVector GetRandomPointInCollisionSphere();

	// Communication
	void GetAllRatsInRange(float InRange, FVector TargetLoc, TArray<ARatCharacter*>& OutRats);
	ARatCharacter* GetFirstRatInRange(float InRange, FVector TargetLoc);
	ARatCharacter* GetClosestRatToManager();

	// RESPONSES/DELEGATE CALLBACKS
	UFUNCTION()
	void RemoveRat(ARatCharacter* InRat);

	UFUNCTION()
	void ReceivePlayerAlert(FVector PlayerLocation, ARatUnit* InRatUnit);

	UFUNCTION()
	void ReceiveCallForHelp(FVector RatLocation, ARatUnit* InRatUnit);
	
	void OrderRatToLocation(ARatCharacter*& InRat, FVector& InLocation);
	void OrderRatToTarget(ARatCharacter* InRat, AActor* InTarget);

	UFUNCTION()
	void ReceivePlayerDead();

	UFUNCTION()
	void RatCompletedOrder(ARatCharacter* InRat);

	// Cosmetic
	UFUNCTION(BlueprintImplementableEvent)
	void RatHasDiedFX();

	UFUNCTION(BlueprintCallable)
	void ReplaceDeadRats();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void AlarmFX(bool bTurnOn);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void DestructionFX();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SignalRatFX(FVector RatLocation);


protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void CancelOrders();

	void InitiateDestruction();

	void DumpRats();

	void ShouldShowHealthBar(bool bShow);

	// RatUnits watched by this Manager
	UPROPERTY(EditInstanceOnly, Category = Manager)
	TArray<ARatUnit*> RatUnits;

	// If a rat has died, its unit will need to be replenished with a new rat. Multiple rats may die from different or the same units during the same interval. This ensures each unit gets a respawn per each rat death.
	UPROPERTY()
	TArray<ARatUnit*> UnitsAwaitingRespawn;

	// Rats watched by this manager
	UPROPERTY(VisibleAnywhere, Category= Manager)
	TArray<ARatCharacter*> ManagedRats;

	// Rats currently executing an order from this manager
	UPROPERTY(VisibleAnywhere, Category = Manager)
	TArray<ARatCharacter*> RatsWithCurrentOrders;

	// Distance this Manager can communicate with rats/spawnpoints
	UPROPERTY(EditAnywhere, Category = Manager)
	float CommunicationRange = 5000.f;

	// Percent radius of collision sphere to spawn destruction VFX 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Cosmetic)
	float DestructionSphereFactor = 0.8f;

	// COMPONENTS
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Cosmetic)
	TObjectPtr<UStaticMeshComponent> StrongholdMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Cosmetic)
	TObjectPtr<USphereComponent> CollisionSphere;

	// MUST BE UNIQUE PER INSTANCE
	UPROPERTY(EditAnywhere)
	FName StrongholdTagName = FName("DefaultTagName");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bWasDestroyed = false;


	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Integrity)
	class UHealthWidgetComponent* HealthWidgetComponent;


private:

	// TIMER
	FTimerHandle DamageTimer;
	void DamageTimerFinished();

	// How long since last damage event do we wait before turning off Damage FX?
	UPROPERTY(EditAnywhere, Category = Stronghold)
	float DamageWaitTime = 6.f;

	// The Managers/Stronghold's "health"
	UPROPERTY(EditAnywhere, Category = Integrity)
	float MaxIntegrity = 500.f;

	UPROPERTY(VisibleAnywhere, Category = Integrity)
	float Integrity = MaxIntegrity;

	UPROPERTY(EditAnywhere, Category = Integrity)
	float CriticalThreshold = 0.25f;

public:	
	FORCEINLINE int GetNumRats() { return ManagedRats.Num(); }

	FORCEINLINE float GetPercentIntegrity() { return Integrity / MaxIntegrity; }
	FORCEINLINE void AddToIntegrity(float InValue) { Integrity = FMath::Clamp(Integrity + InValue, 0.f, MaxIntegrity); }

};
