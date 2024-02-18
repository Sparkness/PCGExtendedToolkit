﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExFuseClusters.h"

#include "Data/PCGExGraphDefinition.h"
#include "Data/Blending/PCGExCompoundBlender.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExFuseClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExFuseClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExFuseClustersContext::~FPCGExFuseClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(CompoundGraph)
	PCGEX_DELETE(GraphBuilder)
	PCGEX_DELETE(PointEdgeIntersections)
	PCGEX_DELETE(EdgeEdgeIntersections)

	PCGEX_DELETE(CompoundPointsBlender)
	PCGEX_DELETE(CompoundEdgesBlender)
}

PCGEX_INITIALIZE_ELEMENT(FuseClusters)

bool FPCGExFuseClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FuseClusters)

	PCGEX_FWD(PointPointIntersectionSettings)
	PCGEX_FWD(PointEdgeIntersectionSettings)
	PCGEX_FWD(EdgeEdgeIntersectionSettings)

	Context->EdgeEdgeIntersectionSettings.ComputeDot();

	Context->GraphMetadataSettings.Grab(Context, Context->PointPointIntersectionSettings);
	Context->GraphMetadataSettings.Grab(Context, Context->PointEdgeIntersectionSettings);
	Context->GraphMetadataSettings.Grab(Context, Context->EdgeEdgeIntersectionSettings);

	PCGEX_FWD(GraphBuilderSettings)

	Context->CompoundGraph = new PCGExGraph::FCompoundGraph(Context->PointPointIntersectionSettings.FuseSettings);

	Context->CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(const_cast<FPCGExBlendingSettings*>(&Settings->PointsBlendingSettings));
	Context->CompoundPointsBlender->AddSources(*Context->MainPoints);

	Context->CompoundEdgesBlender = new PCGExDataBlending::FCompoundBlender(const_cast<FPCGExBlendingSettings*>(&Settings->EdgesBlendingSettings));
	Context->CompoundEdgesBlender->AddSources(*Context->MainEdges);

	return true;
}

bool FPCGExFuseClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFuseClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FuseClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->ConsolidatedPoints = &Context->MainPoints->Emplace_GetRef(PCGExData::EInit::NewOutput);
			Context->SetState(PCGExGraph::State_ProcessingGraph);
		}
		else
		{
			if (!Context->TaggedEdges) { return false; }
			Context->SetState(PCGExGraph::State_ReadyForNextEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (Context->CurrentEdges) { Context->CurrentEdges->Cleanup(); } // Careful as this may impair data blending later on?
		if (!Context->AdvanceEdges(false))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		Context->CurrentEdges->CreateInKeys();

		// Insert current edges into loose graph
		// Note that since we're building from edges only, this skips isolated points altogether

		Context->GetAsyncManager()->Start<PCGExGraphTask::FCompoundGraphInsertEdges>(
			Context->CurrentIO->IOIndex, Context->CurrentIO,
			Context->CompoundGraph, Context->CurrentEdges, &Context->NodeIndicesMap);

		Context->SetAsyncState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		// Create consolidated nodes from compound graph

		const int32 NumCompoundNodes = Context->CompoundGraph->Nodes.Num();
		TArray<FPCGPoint>& MutablePoints = Context->ConsolidatedPoints->GetOut()->GetMutablePoints();

		auto Initialize = [&]() { MutablePoints.SetNum(NumCompoundNodes); };

		auto ProcessNode = [&](int32 Index)
		{
			PCGExGraph::FCompoundNode* CompoundNode = Context->CompoundGraph->Nodes[Index];
			MutablePoints[Index].Transform.SetLocation(CompoundNode->UpdateCenter(Context->CompoundGraph->PointsCompounds, Context->MainPoints));
		};

		if (!Context->Process(Initialize, ProcessNode, NumCompoundNodes)) { return false; }

		// Initiate merging

		Context->CompoundPointsBlender->Merge(
			Context->GetAsyncManager(), Context->ConsolidatedPoints,
			Context->CompoundGraph->PointsCompounds, PCGExSettings::GetDistanceSettings(Context->PointPointIntersectionSettings));

		Context->SetAsyncState(PCGExGraph::State_MergingPointCompounds);
	}

	//TODO : Merge edges, need to create a dummy PointIO for FGraphBuilder

	if (Context->IsState(PCGExGraph::State_MergingPointCompounds))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->CompoundPointsBlender->Write();

		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->ConsolidatedPoints, &Context->GraphBuilderSettings, 6, Context->MainEdges);

		TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
		Context->CompoundGraph->GetUniqueEdges(UniqueEdges);
		Context->CompoundGraph->WriteMetadata(Context->GraphBuilder->Graph->NodeMetadata);
		PCGEX_DELETE(Context->CompoundGraph)

		Context->GraphBuilder->Graph->InsertEdges(UniqueEdges, -1);
		UniqueEdges.Empty();

		if (Settings->bDoPointEdgeIntersection)
		{
			// TODO: Point/edge intersection needs to keep track of original edge IO/Index
			Context->PointEdgeIntersections = new PCGExGraph::FPointEdgeIntersections(Context->GraphBuilder->Graph, Context->ConsolidatedPoints, Context->PointEdgeIntersectionSettings);
			Context->PointEdgeIntersections->FindIntersections(Context);
			Context->SetAsyncState(PCGExGraph::State_FindingPointEdgeIntersections);
		}
		else if (Settings->bDoEdgeEdgeIntersection)
		{
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->ConsolidatedPoints, Context->EdgeEdgeIntersectionSettings);
			Context->EdgeEdgeIntersections->FindIntersections(Context);
			Context->SetAsyncState(PCGExGraph::State_FindingEdgeEdgeIntersections);
		}
		else
		{
			Context->SetAsyncState(PCGExGraph::State_WritingClusters);
		}
	}

	if (Context->IsState(PCGExGraph::State_FindingPointEdgeIntersections))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->PointEdgeIntersections->Insert(); // TODO : Async?
		PCGEX_DELETE(Context->PointEdgeIntersections)

		if (Settings->bDoEdgeEdgeIntersection)
		{
			// TODO : How to retrieve original edge IO/Index for tagging? :/
			// Can be consoldated from CompoundGraph' EdgeIdxCompoupnds + GraphMetadata...?
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->ConsolidatedPoints, Context->EdgeEdgeIntersectionSettings);
			Context->EdgeEdgeIntersections->FindIntersections(Context);
			Context->SetAsyncState(PCGExGraph::State_FindingEdgeEdgeIntersections);
		}
		else
		{
			Context->SetAsyncState(PCGExGraph::State_WritingClusters);
		}
	}

	if (Context->IsState(PCGExGraph::State_FindingEdgeEdgeIntersections))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->EdgeEdgeIntersections->Insert(); // TODO : Async?
		PCGEX_DELETE(Context->EdgeEdgeIntersections)

		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->GraphBuilder->Compile(Context, &Context->GraphMetadataSettings);
		Context->SetAsyncState(PCGExGraph::State_WaitingOnWritingClusters);
		return false;
	}

	if (Context->IsState(PCGExGraph::State_WaitingOnWritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		if (Context->GraphBuilder->bCompiledSuccessfully)
		{
			// Blend edges metadata
			Context->GraphBuilder->Write(Context);
			Context->OutputPoints();
		}

		Context->Done();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
