﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicOperation.h"
#include "PCGExHeuristicsFactoryProvider.h"
#include "PCGExPointsProcessor.h"


#include "Graph/PCGExCluster.h"

#include "PCGExHeuristicAttribute.generated.h"

class UPCGExHeuristicOperation;

USTRUCT(BlueprintType)
struct FPCGExHeuristicAttributeConfig : public FPCGExHeuristicConfigBase
{
	GENERATED_BODY()

	FPCGExHeuristicAttributeConfig() :
		FPCGExHeuristicConfigBase()
	{
	}

	/** Read the data from either vertices or edges */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExClusterComponentSource Source = EPCGExClusterComponentSource::Vtx;

	/** Attribute to read modifier value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGAttributePropertyInputSelector Attribute;
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Attribute")
class UPCGExHeuristicAttribute : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

public:
	virtual void PrepareForCluster(const TSharedPtr<const PCGExCluster::FCluster>& InCluster) override;

	virtual double GetEdgeScore(
		const PCGExCluster::FNode& From,
		const PCGExCluster::FNode& To,
		const PCGExGraph::FEdge& Edge,
		const PCGExCluster::FNode& Seed,
		const PCGExCluster::FNode& Goal,
		const TSharedPtr<PCGEx::FHashLookup> TravelStack) const override;

	virtual void Cleanup() override;

	EPCGExClusterComponentSource Source = EPCGExClusterComponentSource::Vtx;
	FPCGAttributePropertyInputSelector Attribute;

protected:
	TSharedPtr<PCGExData::FPointIO> LastPoints;
	TArray<double> CachedScores;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExHeuristicsFactoryAttribute : public UPCGExHeuristicsFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExHeuristicAttributeConfig Config;

	virtual UPCGExHeuristicOperation* CreateOperation(FPCGExContext* InContext) const override;
	PCGEX_HEURISTIC_FACTORY_BOILERPLATE
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class UPCGExCreateHeuristicAttributeSettings : public UPCGExHeuristicsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		HeuristicsAttribute, "Heuristics : Attribute", "Read a vtx or edge attribute as an heuristic value.",
		FName(GetDisplayName()))
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorHeuristicsAtt; }

#endif
	//~End UPCGSettings

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** Modifier properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicAttributeConfig Config;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

protected:
	virtual bool IsCacheable() const override { return true; }
};
