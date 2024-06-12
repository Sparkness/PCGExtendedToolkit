﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExEdgesProcessor.h"

#include "PCGExPruneClusters.generated.h"


namespace PCGExGeo
{
	class FPointBoxCloud;
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExPruneClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PruneClusters, "Graph : Prune Clusters", "Prune entire clusters.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorGraph; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface

	/** What to do with the selection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ShowOnlyInnerProperties))
	FPCGExDataFilterActionSettings FilterActions;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPruneClustersContext : public FPCGExEdgesProcessorContext
{
	friend class UPCGExPruneClustersSettings;
	friend class FPCGExPruneClustersElement;

	virtual ~FPCGExPruneClustersContext() override;

	PCGExGeo::FPointBoxCloud* BoxCloud = nullptr;
	TArray<bool> ClusterState;

	TArray<PCGExGraph::FIndexedEdge> IndexedEdges;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPruneClustersElement : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPruneClusterTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExPruneClusterTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                       PCGExData::FPointIO* InEdgesIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		EdgesIO(InEdgesIO)
	{
	}

	PCGExData::FPointIO* EdgesIO = nullptr;

	virtual bool ExecuteTask() override;
};
