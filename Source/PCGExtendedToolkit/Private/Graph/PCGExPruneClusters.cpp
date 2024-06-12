﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExPruneClusters.h"

#include "Data/PCGExGraphDefinition.h"
#include "Geometry/PCGExGeoPointBox.h"

#pragma region UPCGSettings interface

PCGExData::EInit UPCGExPruneClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExPruneClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

#pragma endregion

FPCGExPruneClustersContext::~FPCGExPruneClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	IndexedEdges.Empty();
	ClusterState.Empty();

	PCGEX_DELETE(BoxCloud)
}

PCGEX_INITIALIZE_ELEMENT(PruneClusters)

bool FPCGExPruneClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PruneClusters)

	const PCGExData::FPointIO* Targets = Context->TryGetSingleInput(PCGEx::SourceTargetsLabel);

	if (!Targets)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Targets Points."));
		PCGEX_DELETE(Targets)
		return false;
	}

	Context->BoxCloud = new PCGExGeo::FPointBoxCloud(Targets->GetIn());

	PCGEX_DELETE(Targets)

	Context->ClusterState.SetNumUninitialized(Context->MainEdges->Num());
	for (bool& State : Context->ClusterState) { State = false; }

	return true;
}

bool FPCGExPruneClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPruneClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PruneClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		while (Context->AdvancePointsIO())
		{
			if (!Context->TaggedEdges) { continue; }
			for (PCGExData::FPointIO* EdgeIO : Context->TaggedEdges->Entries)
			{
				Context->GetAsyncManager()->Start<FPCGExPruneClusterTask>(EdgeIO->IOIndex, Context->CurrentIO, EdgeIO);
			}
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		PCGEX_WAIT_ASYNC

		TSet<PCGExData::FPointIO*> KeepList;
		TSet<PCGExData::FPointIO*> OmitList;
		
		// TODO : Omit/Keep/Tag
		
		Context->Done();
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndEdges();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExPruneClusterTask::ExecuteTask()
{
	const FPCGExPruneClustersContext* Context = Manager->GetContext<FPCGExPruneClustersContext>();
	PCGEX_SETTINGS(PruneClusters)

	// TODO : Check against BoxCloud
	
	return true;
}
