// A body that moves.
//
// There is no skeletal animation here because there are no animation assets: a person is a handful of
// generated parts, and this swings them. Legs and arms alternate at a cadence set by how fast the actor is
// actually travelling, the torso bobs with the stride, and standing still settles back to a neutral pose.
// It is not motion capture, but it is the difference between someone walking and someone sliding.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

#include "DTGBody.generated.h"

class UAnimSequence;
class UProceduralMeshComponent;
class USkeletalMeshComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DUSTTOGLORY_API UDTGBody : public USceneComponent
{
	GENERATED_BODY()

public:
	UDTGBody();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Builds the parts. Seed picks clothes and skin; 1 and 2 are a hero and a villain. */
	void Build(int32 Seed);

	/** Plays a swing of the arm, for a punch. */
	void Swing();

	/** How fast a stride is at walking pace, in centimetres per second. */
	UPROPERTY(EditAnywhere, Category = "Dust to Glory|Body")
	float WalkSpeed = 320.0f;

private:
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Torso;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Head;

	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> Limbs; // 0,1 legs; 2,3 arms

	/** Epic's mannequin, when Tools/fetch_mannequins.py has copied it in; the parts above are not built then. */
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Figure;

	UPROPERTY()
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY()
	TObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY()
	TObjectPtr<UAnimSequence> JogAnim;

	UPROPERTY()
	TObjectPtr<UAnimSequence> SwingAnim;

	/** Which of idle, walk, jog or swing is playing, so a change of pace restarts the clip once, not every frame. */
	int32 Playing = -1;
	float SwingLeft = 0.0f;

	FVector LastLocation = FVector::ZeroVector;
	float Phase = 0.0f;
	float Speed = 0.0f;
	float Punching = 0.0f;
};
