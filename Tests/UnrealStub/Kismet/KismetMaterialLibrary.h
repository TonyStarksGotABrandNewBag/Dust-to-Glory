#pragma once
#include "CoreMinimal.h"
class UMaterialParameterCollection;
/** Checked against UE 5.8.3's KismetMaterialLibrary.h. */
class UKismetMaterialLibrary
{
public:
	static void SetScalarParameterValue(UObject* WorldContextObject, UMaterialParameterCollection* Collection, FName ParameterName,
		float ParameterValue);
};
