﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOrientOperation.h"
#include "PCGExOrientAverage.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Average")
class UPCGExOrientAverage : public UPCGExOrientOperation
{
	GENERATED_BODY()

public:
	virtual FTransform ComputeOrientation(
		const PCGExData::FPointRef& Point,
		const double DirectionMultiplier) const override
	{
		const FVector A = Path->DirToNextPoint(Point.Index);
		const FVector B = Path->DirToPrevPoint(Point.Index) * -1;
		FTransform OutT = Point.Point->Transform;
		OutT.SetRotation(
			PCGExMath::MakeDirection(
				OrientAxis, FMath::Lerp(A, B, 0.5).GetSafeNormal() * DirectionMultiplier,
				PCGExMath::GetDirection(UpAxis)));
		return OutT;
	}
};
