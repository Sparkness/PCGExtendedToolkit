﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExGoalPicker.h"
#include "Data/PCGExAttributeHelpers.h"
#include "PCGExGoalPickerAttribute.generated.h"

struct FPCGPoint;
class UPCGPointData;

UENUM(BlueprintType)
enum class EPCGExGoalPickAttributeAmount : uint8
{
	Single UMETA(DisplayName = "Single Attribute", Tooltip="Single attribute"),
	List UMETA(DisplayName = "Multiple Attributes", Tooltip="Multiple attributes"),
};

/**
 * 
 */
UCLASS(DisplayName = "Index Attribute")
class PCGEXTENDEDTOOLKIT_API UPCGExGoalPickerAttribute : public UPCGExGoalPicker
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGoalPickAttributeAmount GoalCount = EPCGExGoalPickAttributeAmount::Single;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="GoalCount==EPCGExGoalPickAttributeAmount::Single", EditConditionHides))
	FPCGExInputDescriptor Attribute;
	PCGEx::FLocalSingleFieldGetter AttributeGetter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="GoalCount==EPCGExGoalPickAttributeAmount::List", EditConditionHides, TitleProperty="{TitlePropertyName}"))
	TArray<FPCGExInputDescriptor> Attributes;
	TArray<PCGEx::FLocalSingleFieldGetter> AttributeGetters;

	virtual void PrepareForData(const PCGExData::FPointIO& InSeeds, const PCGExData::FPointIO& InGoals) override;
	virtual int32 GetGoalIndex(const PCGEx::FPointRef& Seed) const override;
	virtual void GetGoalIndices(const PCGEx::FPointRef& Seed, TArray<int32>& OutIndices) const override;
	virtual bool OutputMultipleGoals() const override;
	
#if WITH_EDITOR
	virtual void UpdateUserFacingInfos() override;
#endif
	
};
