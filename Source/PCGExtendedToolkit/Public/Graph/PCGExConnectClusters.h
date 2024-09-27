﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"


#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExConnectClusters.generated.h"

class FPCGExPointIOMerger;

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Bridge Cluster Mode"))
enum class EPCGExBridgeClusterMethod : uint8
{
	Delaunay3D = 0 UMETA(DisplayName = "Delaunay 3D", ToolTip="Uses Delaunay 3D graph to find connections."),
	Delaunay2D = 1 UMETA(DisplayName = "Delaunay 2D", ToolTip="Uses Delaunay 2D graph to find connections."),
	LeastEdges = 2 UMETA(DisplayName = "Least Edges", ToolTip="Ensure all clusters are connected using the least possible number of bridges."),
	MostEdges  = 3 UMETA(DisplayName = "Most Edges", ToolTip="Each cluster will have a bridge to every other cluster"),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExConnectClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ConnectClusters, "Cluster : Connect", "Connects isolated edge clusters by their closest vertices, if they share the same vtx group.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorCluster; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings


	/** Method used to find & insert bridges */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExBridgeClusterMethod BridgeMethod = EPCGExBridgeClusterMethod::Delaunay3D;

	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="BridgeMethod==EPCGExBridgeClusterMethod::Delaunay2D", EditConditionHides))
	FPCGExGeo2DProjectionDetails ProjectionDetails = FPCGExGeo2DProjectionDetails(false);

	/** Meta filter settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails;

private:
	friend class FPCGExConnectClustersElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExConnectClustersContext final : public FPCGExEdgesProcessorContext
{
	friend class FPCGExConnectClustersElement;
	friend class FPCGExCreateBridgeTask;

	FPCGExGeo2DProjectionDetails ProjectionDetails;
	FPCGExCarryOverDetails CarryOverDetails;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExConnectClustersElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExBridgeClusters
{
	class FProcessor final : public PCGExClusterMT::TClusterProcessor<FPCGExConnectClustersContext, UPCGExConnectClustersSettings>
	{
	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TClusterProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};

	class FProcessorBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
	public:
		TSharedPtr<PCGExData::FPointIO> ConsolidatedEdges;
		TSharedPtr<FPCGExPointIOMerger> Merger;
		TSet<uint64> Bridges;

		FProcessorBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);

		virtual void OnProcessingPreparationComplete() override;
		virtual void Process() override;
		virtual bool PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCreateBridgeTask final : public PCGExMT::FPCGExTask
	{
	public:
		FPCGExCreateBridgeTask(
			const TSharedPtr<PCGExData::FPointIO>& InPointIO,
			FProcessorBatch* InBatch,
			PCGExCluster::FCluster* A,
			PCGExCluster::FCluster* B) :
			FPCGExTask(InPointIO),
			Batch(InBatch),
			ClusterA(A),
			ClusterB(B)
		{
		}

		FProcessorBatch* Batch = nullptr;

		PCGExCluster::FCluster* ClusterA = nullptr;
		PCGExCluster::FCluster* ClusterB = nullptr;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}
