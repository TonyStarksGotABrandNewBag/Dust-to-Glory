#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
class USkeletalMesh;
class UAnimationAsset;
enum class EVisibilityBasedAnimTickOption : uint8
{
	AlwaysTickPoseAndRefreshBones,
	AlwaysTickPose,
	OnlyTickMontagesWhenNotRendered,
	OnlyTickPoseWhenRendered,
};
/** Checked against UE 5.8.3's SkinnedMeshComponent.h and SkeletalMeshComponent.h. */
class USkeletalMeshComponent : public UMeshComponent
{
public:
	virtual void SetSkeletalMesh(USkeletalMesh* NewMesh, bool bReinitPose = true);
	void SetLeaderPoseComponent(USkeletalMeshComponent* NewLeaderBoneComponent, bool bForceUpdate = false, bool bInFollowerShouldTickPose = false);
	EVisibilityBasedAnimTickOption VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	void PlayAnimation(UAnimationAsset* NewAnimToPlay, bool bLooping);
	void SetPlayRate(float Rate);
	bool IsPlaying() const;
	class UAnimInstance* GetAnimInstance() const;
};
