﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathsToEdgeClusters.h"
#include "Graph/PCGExGraph.h"
#include "Data/Blending/PCGExCompoundBlender.h"

#define LOCTEXT_NAMESPACE "PCGExPathsToEdgeClustersElement"
#define PCGEX_NAMESPACE BuildCustomGraph

UPCGExPathsToEdgeClustersSettings::UPCGExPathsToEdgeClustersSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExPathsToEdgeClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinClustersOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinClustersOutput.Tooltip = FTEXT("Point data representing edges.");
#endif


	return PinProperties;
}

#if WITH_EDITOR
void UPCGExPathsToEdgeClustersSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExPathsToEdgeClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExPathsToEdgeClustersSettings::GetMainInputLabel() const { return PCGExGraph::SourcePathsLabel; }

FName UPCGExPathsToEdgeClustersSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(PathsToEdgeClusters)

FPCGExPathsToEdgeClustersContext::~FPCGExPathsToEdgeClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(CompoundGraph)
	PCGEX_DELETE(GraphBuilder)
	PCGEX_DELETE(PointEdgeIntersections)
	PCGEX_DELETE(EdgeEdgeIntersections)

	PCGEX_DELETE(CompoundPointsBlender)
}

bool FPCGExPathsToEdgeClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathsToEdgeClusters)

	PCGEX_FWD(PointPointIntersectionSettings)
	PCGEX_FWD(PointEdgeIntersectionSettings)
	PCGEX_FWD(EdgeEdgeIntersectionSettings)

	Context->EdgeEdgeIntersectionSettings.ComputeDot();

	Context->GraphMetadataSettings.Grab(Context, Context->PointPointIntersectionSettings);
	Context->GraphMetadataSettings.Grab(Context, Context->PointEdgeIntersectionSettings);
	Context->GraphMetadataSettings.Grab(Context, Context->EdgeEdgeIntersectionSettings);

	Context->CompoundPointsBlender = new PCGExDataBlending::FCompoundBlender(const_cast<FPCGExBlendingSettings*>(&Settings->PointsBlendingSettings));
	Context->CompoundPointsBlender->AddSources(*Context->MainPoints);

	PCGEX_FWD(GraphBuilderSettings)

	Context->CompoundGraph = new PCGExGraph::FCompoundGraph(Context->PointPointIntersectionSettings.FuseSettings);

	return true;
}


bool FPCGExPathsToEdgeClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathsToEdgeClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathsToEdgeClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			if (Context->CompoundGraph->Nodes.IsEmpty()) { return true; }
			Context->ConsolidatedPoints = &Context->MainPoints->Emplace_GetRef(PCGExData::EInit::NewOutput);
			Context->SetState(PCGExGraph::State_ProcessingGraph);
		}
		else
		{
			// Fuse points

			Context->GetAsyncManager()->Start<FPCGExInsertPathToCompoundGraphTask>(
				Context->CurrentIO->IOIndex, Context->CurrentIO, Context->CompoundGraph, Settings->bClosedPath);

			Context->SetAsyncState(PCGExMT::State_ProcessingPoints);
		}
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingGraph))
	{
		// Create consolidated set of points from

		const int32 NumCompoundNodes = Context->CompoundGraph->Nodes.Num();
		const TArray<PCGExData::FPointIO*>& Sources = Context->MainPoints->Pairs;

		auto Initialize = [&]()
		{
			TArray<FPCGPoint>& MutablePoints = Context->ConsolidatedPoints->GetOut()->GetMutablePoints();
			MutablePoints.SetNum(NumCompoundNodes);
		};

		auto ProcessNode = [&](int32 Index)
		{
			Context->ConsolidatedPoints->GetMutablePoint(Index).Transform.SetLocation(
				Context->CompoundGraph->Nodes[Index]->UpdateCenter(Context->CompoundGraph->PointsCompounds, Context->MainPoints));
		};

		if (!Context->Process(Initialize, ProcessNode, NumCompoundNodes)) { return false; }

		// Initiate merging

		Context->CompoundPointsBlender->Merge(
			Context->GetAsyncManager(), Context->ConsolidatedPoints,
			Context->CompoundGraph->PointsCompounds, PCGExSettings::GetDistanceSettings(Context->PointPointIntersectionSettings));

		Context->SetAsyncState(PCGExData::State_MergingData);
	}

	if (Context->IsState(PCGExData::State_MergingData))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->CompoundPointsBlender->Write();

		// Build final graph from compound graph

		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->ConsolidatedPoints, &Context->GraphBuilderSettings, 4);

		TArray<PCGExGraph::FUnsignedEdge> UniqueEdges;
		Context->CompoundGraph->GetUniqueEdges(UniqueEdges);
		Context->CompoundGraph->WriteMetadata(Context->GraphBuilder->Graph->NodeMetadata);
		
		Context->GraphBuilder->Graph->InsertEdges(UniqueEdges, -1); //TODO : valid IOIndex from CompoundGraph
		UniqueEdges.Empty();

		if (Settings->bDoPointEdgeIntersection)
		{
			Context->PointEdgeIntersections = new PCGExGraph::FPointEdgeIntersections(Context->GraphBuilder->Graph, Context->CompoundGraph, Context->ConsolidatedPoints, Context->PointEdgeIntersectionSettings);
			Context->PointEdgeIntersections->FindIntersections(Context);
			Context->SetAsyncState(PCGExGraph::State_FindingPointEdgeIntersections);
		}
		else if (Settings->bDoEdgeEdgeIntersection)
		{
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->CompoundGraph, Context->ConsolidatedPoints, Context->EdgeEdgeIntersectionSettings);
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
			Context->EdgeEdgeIntersections = new PCGExGraph::FEdgeEdgeIntersections(Context->GraphBuilder->Graph, Context->CompoundGraph, Context->ConsolidatedPoints, Context->EdgeEdgeIntersectionSettings);
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
			Context->GraphBuilder->Write(Context);
			Context->OutputPoints();
		}

		Context->Done();
	}

	return Context->IsDone();
}

bool FPCGExInsertPathToCompoundGraphTask::ExecuteTask()
{
	const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
	const int32 NumPoints = InPoints.Num();

	if (NumPoints < 2) { return false; }

	for (int i = 0; i < NumPoints; i++)
	{
		PCGExGraph::FCompoundNode* CurrentVtx = Graph->GetOrCreateNode(InPoints[i], TaskIndex, i);

		if (const int32 PrevIndex = i - 1;
			InPoints.IsValidIndex(PrevIndex))
		{
			PCGExGraph::FCompoundNode* OtherVtx = Graph->GetOrCreateNode(InPoints[PrevIndex], TaskIndex, PrevIndex);
			CurrentVtx->Add(OtherVtx);
		}

		if (const int32 NextIndex = i + 1;
			InPoints.IsValidIndex(NextIndex))
		{
			PCGExGraph::FCompoundNode* OtherVtx = Graph->GetOrCreateNode(InPoints[NextIndex], TaskIndex, NextIndex);
			CurrentVtx->Add(OtherVtx);
		}
	}

	if (bJoinFirstAndLast)
	{
		// Join
		const int32 LastIndex = NumPoints - 1;
		Graph->CreateBridge(
			InPoints[0], TaskIndex, 0,
			InPoints[LastIndex], TaskIndex, LastIndex);
	}

	return true;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
