#pragma once
#include "CoreMinimal.h"
class UTexture;
/** Checked against UE 5.8.3's MaterialTypes.h: the name is all this project uses. */
struct FMaterialParameterInfo
{
	FName Name;
	FMaterialParameterInfo() = default;
	FMaterialParameterInfo(FName InName) : Name(InName) {}
	FMaterialParameterInfo(const TCHAR* InName) : Name(InName) {}
};
class UMaterialInterface : public UObject
{
};
class UMaterialInstance : public UMaterialInterface
{
public:
	bool GetTextureParameterValue(const FMaterialParameterInfo& ParameterInfo, UTexture*& OutValue, bool bOveriddenOnly = false) const
	{
		(void)ParameterInfo; (void)bOveriddenOnly; OutValue = nullptr; return false;
	}
};
class UMaterial : public UMaterialInterface
{
};
class UMaterialInstanceDynamic : public UMaterialInstance
{
public:
	void SetVectorParameterValue(FName Name, const FLinearColor& Value) { (void)Name; (void)Value; }
	void SetScalarParameterValue(FName Name, float Value) { (void)Name; (void)Value; }
	void SetTextureParameterValue(FName Name, UTexture* Value) { (void)Name; (void)Value; }
	static UMaterialInstanceDynamic* Create(UMaterialInterface* Parent, UObject* Outer)
	{
		(void)Parent;
		(void)Outer;
		return nullptr;
	}
};
