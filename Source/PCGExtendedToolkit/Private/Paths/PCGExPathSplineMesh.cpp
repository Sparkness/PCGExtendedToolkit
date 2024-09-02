﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathSplineMesh.h"

#include "PCGExHelpers.h"
#include "PCGExManagedResource.h"
#include "AssetSelectors/PCGExInternalCollection.h"
#include "Paths/PCGExPaths.h"
#include "Paths/Tangents/PCGExZeroTangents.h"

#define LOCTEXT_NAMESPACE "PCGExPathSplineMeshElement"
#define PCGEX_NAMESPACE BuildCustomGraph

PCGEX_INITIALIZE_ELEMENT(PathSplineMesh)

TArray<FPCGPinProperties> UPCGExPathSplineMeshSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	if (CollectionSource == EPCGExCollectionSource::AttributeSet)
	{
		PCGEX_PIN_PARAM(PCGExAssetCollection::SourceAssetCollection, "Attribute set to be used as collection.", Required, {})
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathSplineMeshSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	// No output
	return PinProperties;
}

PCGExData::EInit UPCGExPathSplineMeshSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGExPathSplineMeshContext::~FPCGExPathSplineMeshContext()
{
	PCGEX_TERMINATE_ASYNC
}

void FPCGExPathSplineMeshContext::RegisterAssetDependencies()
{
	FPCGExPathProcessorContext::RegisterAssetDependencies();

	PCGEX_SETTINGS_LOCAL(PathSplineMesh)

	if (Settings->CollectionSource == EPCGExCollectionSource::Asset)
	{
		MainCollection = Settings->AssetCollection.Get();
		if (!MainCollection) { RequiredAssets.Add(Settings->AssetCollection.ToSoftObjectPath()); }
	}
	else
	{
		// Only load assets for internal collections
		// since we need them for staging
		MainCollection = GetDefault<UPCGExInternalCollection>()->GetCollectionFromAttributeSet(
			this, PCGExAssetCollection::SourceAssetCollection,
			Settings->AttributeSetDetails, false);

		if (MainCollection) { MainCollection->GetAssetPaths(RequiredAssets, PCGExAssetCollection::ELoadingFlags::Recursive); }
	}
}

bool FPCGExPathSplineMeshElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathSplineMesh)

	if (!Settings->bTangentsFromAttributes)
	{
		PCGEX_OPERATION_BIND(Tangents, UPCGExZeroTangents)
		Context->Tangents->bClosedPath = Settings->bClosedPath;
	}

	/*
	if (Settings->bPerSegmentTargetActor)
	{
		PCGEX_VALIDATE_NAME(Settings->TargetActorAttributeName)
	}
	*/

	if (Settings->CollectionSource == EPCGExCollectionSource::Asset)
	{
		Context->MainCollection = Settings->AssetCollection.LoadSynchronous();
	}
	else
	{
		if (Context->MainCollection)
		{
			// Internal collection, assets have been loaded at this point
			Context->MainCollection->RebuildStagingData(true);
		}
	}

	if (!Context->MainCollection)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing asset collection."));
		return false;
	}

	Context->MainCollection->LoadCache(); // Make sure to load the stuff

	return true;
}

bool FPCGExPathSplineMeshElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathSplineMeshElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathSplineMesh)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathSplineMesh::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bInvalidInputs = true;
					Entry->InitializeOutput(PCGExData::EInit::Forward);
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExPathSplineMesh::FProcessor>* NewBatch)
			{
				NewBatch->PrimaryOperation = Context->Tangents;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to write tangents to."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainBatch->Output();

	// Execute PostProcess Functions
	if (!Context->NotifyActors.IsEmpty())
	{
		TArray<AActor*> NotifyActors = Context->NotifyActors.Array();
		for (AActor* TargetActor : NotifyActors)
		{
			for (UFunction* Function : PCGExHelpers::FindUserFunctions(TargetActor->GetClass(), Settings->PostProcessFunctionNames, {UPCGExFunctionPrototypes::GetPrototypeWithNoParams()}, Context))
			{
				TargetActor->ProcessEvent(Function, nullptr);
			}
		}
	}

	return Context->TryComplete();
}

namespace PCGExPathSplineMesh
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(Helper)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathSplineMesh)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		PointDataFacade->bSupportsDynamic = true;

		Helper = new PCGExAssetCollection::FDistributionHelper(LocalTypedContext->MainCollection, Settings->DistributionSettings);
		if (!Helper->Init(Context, PointDataFacade)) { return false; }

		if (Settings->bTangentsFromAttributes)
		{
			Tangents = Cast<UPCGExTangentsOperation>(PrimaryOperation);
			Tangents->PrepareForData(PointDataFacade);
		}
		else
		{
			ArriveReader = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->Arrive);
			LeaveReader = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->Leave);
		}

		LastIndex = PointIO->GetNum() - 1;

		PCGEX_SET_NUM_UNINITIALIZED(Segments, LastIndex)

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		if (Index == LastIndex) { return; } // Ignore last index, only used for maths reasons

		Segments[Index] = PCGExPaths::FSplineMeshSegment();
		PCGExPaths::FSplineMeshSegment& Segment = Segments[Index];

		const FPCGExAssetStagingData* StagingData = nullptr;

		const int32 Seed = PCGExRandom::GetSeedFromPoint(
			Helper->Details.SeedComponents, Point,
			Helper->Details.LocalSeed, LocalSettings, LocalTypedContext->SourceComponent.Get());

		Helper->GetStaging(StagingData, Index, Seed);
		Segment.AssetStaging = StagingData;

		const FPCGPoint& NextPoint = PointIO->GetInPoint(Index + 1);
		const FTransform& StartTransform = Point.Transform;
		const FTransform& EndTransform = NextPoint.Transform;

		FVector Scale = StartTransform.GetScale3D();
		Segment.Params.StartPos = StartTransform.GetLocation();
		Segment.Params.StartScale = FVector2D(Scale.Y, Scale.Z);
		Segment.Params.StartRoll = StartTransform.GetRotation().Rotator().Roll;

		Scale = EndTransform.GetScale3D();
		Segment.Params.EndPos = EndTransform.GetLocation();
		Segment.Params.EndScale = FVector2D(Scale.Y, Scale.Z);
		Segment.Params.EndRoll = EndTransform.GetRotation().Rotator().Roll;
	}

	void FProcessor::CompleteWork()
	{
	}

	void FProcessor::Output()
	{
		// TODO : Resolve per-point target actor...? irk.
		AActor* TargetActor = LocalSettings->TargetActor.Get() ? LocalSettings->TargetActor.Get() : Context->GetTargetActor(nullptr);

		if (!TargetActor)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Invalid target actor."));
			return;
		}

		UPCGComponent* Comp = Context->SourceComponent.Get();

		bool bEncounteredInvalidStaging = false;
		for (const PCGExPaths::FSplineMeshSegment& SegmentParams : Segments)
		{
			if (!SegmentParams.AssetStaging)
			{
				// Warning
				bEncounteredInvalidStaging = true;
				continue;
			}

			if (!SegmentParams.AssetStaging->LoadSynchronous<UStaticMesh>())
			{
				// Staging exist, just has empty/unloadable data.
				continue;
			}

			if (UPCGExManagedSplineMeshComponent::GetOrCreate(TargetActor, Comp, LocalSettings->UID, SegmentParams, true))
			{
				LocalTypedContext->NotifyActors.Add(TargetActor);
			}
		}

		if (bEncounteredInvalidStaging)
		{
			// TODO : Throw
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
