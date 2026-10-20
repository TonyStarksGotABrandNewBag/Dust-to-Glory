#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
class USkeletalMesh;
class UAnimationAsset;
/** Checked against UE 5.8.3's SkinnedMeshComponent.h and SkeletalMeshComponent.h. */
class USkeletalMeshComponent : public UPrimitiveComponent
{
public:
	virtual void SetSkeletalMesh(USkeletalMesh* NewMesh, bool bReinitPose = true);
	void PlayAnimation(UAnimationAsset* NewAnimToPlay, bool bLooping);
	void SetPlayRate(float Rate);
	bool IsPlaying() const;
	class UAnimInstance* GetAnimInstance() const;
};
