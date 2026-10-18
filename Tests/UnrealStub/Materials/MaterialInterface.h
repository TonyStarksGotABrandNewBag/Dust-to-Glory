#pragma once
#include "CoreMinimal.h"
class UMaterialInterface : public UObject
{
};
class UMaterial : public UMaterialInterface
{
};
class UMaterialInstanceDynamic : public UMaterialInterface
{
public:
	void SetVectorParameterValue(FName Name, const FLinearColor& Value) { (void)Name; (void)Value; }
	void SetScalarParameterValue(FName Name, float Value) { (void)Name; (void)Value; }
	static UMaterialInstanceDynamic* Create(UMaterialInterface* Parent, UObject* Outer)
	{
		(void)Parent;
		(void)Outer;
		return nullptr;
	}
};
