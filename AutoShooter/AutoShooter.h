// Copyright Alyssa Hunter 2025.

#pragma once

#include "CoreMinimal.h"
#include "../Interface/SaveableInterface.h"
#include "GameFramework/Pawn.h"
#include "AutoShooter.generated.h"

class USphereComponent;
class UPawnSensingComponent;
class AProjectile;
class UZappyCats_SaveGame;

/*
Use this class for any passive object in world that should sense player and shoot based on parameters. For instance a cannon or an arrow-trap. 
*/

UCLASS()
class ZAPPYCATS_API AAutoShooter : public APawn, public ISaveableInterface
{
	GENERATED_BODY()
	
public:	
	AAutoShooter();
	virtual void Tick(float DeltaTime) override;

	// SAVEABLE INTERFACE
	virtual void SaveData(UZappyCats_SaveGame& OutSaveGame) override;
	virtual void LoadData(UZappyCats_SaveGame* SaveGameRef) override;
	virtual bool IsLevelRequirement() override;

	void Hide();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void PrepareToFireFX();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void CancelFX();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void FireFX();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void DestructionFX();

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	void ShouldShowHealthBar(bool bShow);

	UFUNCTION(BlueprintCallable)
	bool TargetInRange();

	UFUNCTION(BlueprintCallable)
	bool HasLineOfSight();

	UFUNCTION(BlueprintCallable)
	bool TargetWithinArch();

	UFUNCTION()
	void WaitForNextAttack();

	UFUNCTION()
	void ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, class AController* InstigatorController, AActor* DamageCauser);

	UFUNCTION()
	void PawnSeen(APawn* SeenPawn);

	UFUNCTION(BlueprintCallable)
	void Attack();

	// COMPONENTS/PROPERTIES
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USphereComponent* CollisionSphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USceneComponent* MuzzleLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UPawnSensingComponent* PawnSensing;

	UPROPERTY(VisibleAnywhere)
	class UHealthWidgetComponent* HealthWidgetComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AutoShooter)
	TSubclassOf<AProjectile> ProjectileClass;

	// Fire-arch-range of muzzle. IE We don't want to fire to the side of the muzzle because it looks ridiculous.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AutoShooter)
	float LookAtArchLimit = 45.f;

	// Range this actor can fire
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AutoShooter)
	float AttackRange = 4000.f;

	// Minimum Time between fires
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AutoShooter)
	float FireRateMin = 1.f;

	// Maximum Time between fires
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AutoShooter)
	float FireRateMax = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AutoShooter)
	float MaxHealth = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AutoShooter)
	float Health = MaxHealth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	AActor* Target;

	UPROPERTY()
	FTimerHandle AttackTimer;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName DestructibleTag;

public:
	FORCEINLINE float GetPercentHealth() { return Health / MaxHealth; }
	FORCEINLINE void AddToHealth(float InValue) { Health = FMath::Clamp(Health + InValue, 0.f, MaxHealth); }
	FORCEINLINE FName GetDestructibleTagName() { return DestructibleTag; }
};
