﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/Heuristics/PCGExHeuristicAttribute.h"


#include "Graph/Pathfinding/PCGExPathfinding.h"

#define LOCTEXT_NAMESPACE "PCGExCreateHeuristicAttribute"
#define PCGEX_NAMESPACE CreateHeuristicAttribute

void UPCGExHeuristicAttribute::PrepareForCluster(const TSharedPtr<const PCGExCluster::FCluster>& InCluster)
{
	Super::PrepareForCluster(InCluster);

	const TSharedPtr<PCGExData::FPointIO> InPoints = Source == EPCGExClusterComponentSource::Vtx ? InCluster->VtxIO.Pin() : InCluster->EdgesIO.Pin();
	const TSharedPtr<PCGExData::FFacade> DataFacade = Source == EPCGExClusterComponentSource::Vtx ? PrimaryDataFacade : SecondaryDataFacade;

	if (LastPoints == InPoints) { return; }

	const int32 NumPoints = Source == EPCGExClusterComponentSource::Vtx ? InCluster->Nodes->Num() : InPoints->GetNum();

	LastPoints = InPoints;
	CachedScores.SetNumZeroed(NumPoints);

	const TSharedPtr<PCGExData::TBuffer<double>> ModifiersCache = DataFacade->GetBroadcaster<double>(Attribute, true);

	if (!ModifiersCache)
	{
		PCGEX_LOG_INVALID_SELECTOR_C(Context, "Heuristic", Attribute)
		return;
	}

	const double MinValue = ModifiersCache->Min;
	const double MaxValue = ModifiersCache->Max;

	const double OutMin = bInvert ? 1 : 0;
	const double OutMax = bInvert ? 0 : 1;

	const double Factor = ReferenceWeight * WeightFactor;

	if (Source == EPCGExClusterComponentSource::Vtx)
	{
		for (const PCGExCluster::FNode& Node : (*InCluster->Nodes))
		{
			const double NormalizedValue = PCGExMath::Remap(ModifiersCache->Read(Node.PointIndex), MinValue, MaxValue, OutMin, OutMax);
			CachedScores[Node.Index] += FMath::Max(0, ScoreCurve->Eval(NormalizedValue)) * Factor;
		}
	}
	else
	{
		for (int i = 0; i < NumPoints; i++)
		{
			const double NormalizedValue = PCGExMath::Remap(ModifiersCache->Read(i), MinValue, MaxValue, OutMin, OutMax);
			CachedScores[i] += FMath::Max(0, ScoreCurve->Eval(NormalizedValue)) * Factor;
		}
	}
}

double UPCGExHeuristicAttribute::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal,
	const TSharedPtr<PCGEx::FHashLookup> TravelStack) const
{
	return CachedScores[Source == EPCGExClusterComponentSource::Edge ? Edge.PointIndex : To.Index];
}

void UPCGExHeuristicAttribute::Cleanup()
{
	CachedScores.Empty();
	LastPoints.Reset();
	Super::Cleanup();
}

UPCGExHeuristicOperation* UPCGExHeuristicsFactoryAttribute::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExHeuristicAttribute* NewOperation = InContext->ManagedObjects->New<UPCGExHeuristicAttribute>();
	PCGEX_FORWARD_HEURISTIC_CONFIG
	NewOperation->Attribute = Config.Attribute;
	return NewOperation;
}

PCGEX_HEURISTIC_FACTORY_BOILERPLATE_IMPL(Attribute, {})

UPCGExFactoryData* UPCGExCreateHeuristicAttributeSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExHeuristicsFactoryAttribute* NewFactory = InContext->ManagedObjects->New<UPCGExHeuristicsFactoryAttribute>();
	PCGEX_FORWARD_HEURISTIC_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExCreateHeuristicAttributeSettings::GetDisplayName() const
{
	return TEXT("HX : ") + PCGEx::GetSelectorDisplayName(Config.Attribute)
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
