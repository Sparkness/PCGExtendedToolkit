﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/States/PCGExFlagNodes.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Graph/PCGExCluster.h"
#include "Graph/States/PCGExClusterStates.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE FlagNodes

int32 UPCGExFlagNodesSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }

PCGExData::EInit UPCGExFlagNodesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExFlagNodesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

TArray<FPCGPinProperties> UPCGExFlagNodesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExCluster::SourceNodeStateLabel, "Node states.", Required, {})
	return PinProperties;
}

FPCGExFlagNodesContext::~FPCGExFlagNodesContext()
{
	PCGEX_TERMINATE_ASYNC
	StateFactories.Empty();
}

PCGEX_INITIALIZE_ELEMENT(FlagNodes)

bool FPCGExFlagNodesElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FlagNodes)

	if (!PCGExFactories::GetInputFactories(Context, PCGExCluster::SourceNodeStateLabel, Context->StateFactories, {PCGExFactories::EType::StateNode}, true))
	{
		return false;
	}

	Context->StateFactories.Sort([](const UPCGExFilterFactoryBase& A, const UPCGExFilterFactoryBase& B) { return A.Priority < B.Priority; });
	return true;
}

bool FPCGExFlagNodesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFlagNodesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FlagNodes)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExFindNodeState::FProcessorBatch>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExFindNodeState::FProcessorBatch* NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExFindNodeState
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(StateManager)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FlagNodes)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		ExpandedNodes = Cluster->ExpandedNodes;

		if (!ExpandedNodes)
		{
			ExpandedNodes = Cluster->GetExpandedNodes(false);
			bBuildExpandedNodes = true;
		}

		Cluster->ComputeEdgeLengths();

		StateManager = new PCGExClusterStates::FStateManager(StateFlags, Cluster, VtxDataCache, EdgeDataCache);
		StateManager->Init(Context, TypedContext->StateFactories);

		if (bBuildExpandedNodes) { StartParallelLoopForRange(NumNodes); }
		else { StartParallelLoopForNodes(); }

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration)
	{
		(*ExpandedNodes)[Iteration] = new PCGExCluster::FExpandedNode(Cluster, Iteration);
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node)
	{
		StateManager->Test(Node);
	}

	void FProcessor::CompleteWork()
	{
		if (bBuildExpandedNodes) { StartParallelLoopForNodes(); }
	}

	void FProcessor::Write()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FlagNodes)
	}

	//////// BATCH

	FProcessorBatch::FProcessorBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, const TArrayView<PCGExData::FPointIO*> InEdges):
		TBatch(InContext, InVtx, InEdges)
	{
	}

	FProcessorBatch::~FProcessorBatch()
	{
		StateFlags = nullptr;
	}

	bool FProcessorBatch::PrepareProcessing()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(FlagNodes)

		if (!TBatch::PrepareProcessing()) { return false; }

		PCGExDataCaching::FCache<int64>* Cache = VtxDataCache->GetOrCreateWriter(Settings->FlagAttribute, Settings->InitialFlags.Get(), false, false);
		PCGEx::TFAttributeWriter<int64>* Writer = Cache->PrepareWriter();
		StateFlags = &Writer->Values;

		return true;
	}

	bool FProcessorBatch::PrepareSingle(FProcessor* ClusterProcessor)
	{
		ClusterProcessor->StateFlags = StateFlags;
		return true;
	}

	void FProcessorBatch::Write()
	{
		VtxDataCache->Write();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
