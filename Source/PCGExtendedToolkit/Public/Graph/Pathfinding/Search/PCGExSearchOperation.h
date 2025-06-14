﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExInstancedFactory.h"
#include "PCGExOperation.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "UObject/Object.h"
#include "PCGExSearchOperation.generated.h"

namespace PCGExPathfinding
{
	struct FExtraWeights;
}

class FPCGExHeuristicOperation;

namespace PCGExCluster
{
	class FCluster;
}

class FPCGExSearchOperation : public FPCGExOperation
{
public:
	bool bEarlyExit = true;
	PCGExCluster::FCluster* Cluster = nullptr;

	virtual void PrepareForCluster(PCGExCluster::FCluster* InCluster);
	virtual bool ResolveQuery(
		const TSharedPtr<PCGExPathfinding::FPathQuery>& InQuery,
		const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& Heuristics,
		const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback = nullptr) const;
};

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExSearchInstancedFactory : public UPCGExInstancedFactory
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override;

	virtual TSharedPtr<FPCGExSearchOperation> CreateOperation() const
	PCGEX_NOT_IMPLEMENTED_RET(CreateOperation(), nullptr);

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bEarlyExit = true;
};
