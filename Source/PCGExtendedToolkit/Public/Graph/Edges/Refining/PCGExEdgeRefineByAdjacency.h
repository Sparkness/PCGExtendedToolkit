﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExEdgeRefineOperation.h"
#include "Graph/PCGExCluster.h"

#include "PCGExEdgeRefineByAdjacency.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Refine Edge Threshold Mode"))
enum class EPCGExRefineEdgeThresholdMode : uint8
{
	Sum  = 0 UMETA(DisplayName = "Sum", Tooltip="The sum of adjacencies must be below the specified threshold"),
	Any  = 1 UMETA(DisplayName = "Any Endpoint", Tooltip="At least one endpoint adjacency count must be below the specified threshold"),
	Both = 2 UMETA(DisplayName = "Both Endpoints", Tooltip="Both endpoint adjacency count must be below the specified threshold"),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine by Adjacency"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeRefineByAdjacency : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeRefineByAdjacency* TypedOther = Cast<UPCGExEdgeRefineByAdjacency>(Other))
		{
			Threshold = TypedOther->Threshold;
			Mode = TypedOther->Mode;
			Comparison = TypedOther->Comparison;
			bInvert = TypedOther->bInvert;
		}
	}

	virtual bool RequiresIndividualEdgeProcessing() override { return true; }
	virtual bool GetDefaultEdgeValidity() override { return !bInvert; }

	virtual void ProcessEdge(PCGExGraph::FIndexedEdge& Edge) override
	{
		Super::ProcessEdge(Edge);

		const PCGExCluster::FNode* From = Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[Edge.Start];
		const PCGExCluster::FNode* To = Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[Edge.End];

		if (Mode == EPCGExRefineEdgeThresholdMode::Both)
		{
			if (PCGExCompare::Compare(Comparison, From->Adjacency.Num(), Threshold) && PCGExCompare::Compare(Comparison, To->Adjacency.Num(), Threshold, Tolerance)) { return; }
		}
		else if (Mode == EPCGExRefineEdgeThresholdMode::Any)
		{
			if (PCGExCompare::Compare(Comparison, From->Adjacency.Num(), Threshold) || PCGExCompare::Compare(Comparison, To->Adjacency.Num(), Threshold, Tolerance)) { return; }
		}
		else if (Mode == EPCGExRefineEdgeThresholdMode::Sum)
		{
			if (PCGExCompare::Compare(Comparison, (From->Adjacency.Num() + To->Adjacency.Num()), Threshold, Tolerance)) { return; }
		}

		FPlatformAtomics::InterlockedExchange(&Edge.bValid, bInvert ? 1 : 0);
	}

	/** The number of connection endpoints must have to be considered a Bridge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin="1"))
	int32 Threshold = 2;

	/** How should we check if the threshold is reached. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExRefineEdgeThresholdMode Mode = EPCGExRefineEdgeThresholdMode::Sum;
	
	/** Comparison check */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExComparison Comparison = EPCGExComparison::StrictlyGreater;

	/** Rounding mode for approx. comparison modes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides))
	int32 Tolerance = 0;
	
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

protected:
	virtual void ApplyOverrides() override
	{
		Super::ApplyOverrides();
		
		PCGEX_OVERRIDE_OPERATION_PROPERTY(Threshold, "Refine/Threshold")
		PCGEX_OVERRIDE_OPERATION_PROPERTY(Tolerance, "Refine/Tolerance")
		PCGEX_OVERRIDE_OPERATION_PROPERTY(bInvert, "Refine/Invert")
	}
};
