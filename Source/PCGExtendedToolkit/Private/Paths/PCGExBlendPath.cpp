﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExBlendPath.h"


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExBlendPathElement"
#define PCGEX_NAMESPACE BlendPath

UPCGExBlendPathSettings::UPCGExBlendPathSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportClosedLoops = false;
}

PCGEX_INITIALIZE_ELEMENT(BlendPath)

bool FPCGExBlendPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BlendPath)

	return true;
}

bool FPCGExBlendPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBlendPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BlendPath)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		// TODO : Skip completion

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBlendPath::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				PCGEX_SKIP_INVALID_PATH_ENTRY
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExBlendPath::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to blend."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExBlendPath
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBlendPath::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		if (Settings->BlendOver == EPCGExBlendOver::Fixed)
		{
			LerpCache = Settings->GetValueSettingLerp();
			if (!LerpCache->Init(Context, PointDataFacade)) { return false; }
		}

		TArray<FPCGPoint>& OutPoints = PointDataFacade->GetOut()->GetMutablePoints();
		MaxIndex = OutPoints.Num() - 1;

		Start = MakeShared<PCGExData::FPointRef>(PointDataFacade->Source->GetInPoint(0), 0);
		End = MakeShared<PCGExData::FPointRef>(PointDataFacade->Source->GetInPoint(MaxIndex), MaxIndex);

		MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&Settings->BlendingSettings);
		MetadataBlender->PrepareForData(PointDataFacade);

		if (Settings->BlendOver == EPCGExBlendOver::Distance)
		{
			Metrics = PCGExPaths::FPathMetrics(OutPoints[0].Transform.GetLocation());
			PCGEx::InitArray(Length, PointDataFacade->GetNum());
			for (int i = 0; i < PointDataFacade->GetNum(); i++) { Length[i] = Metrics.Add(OutPoints[i].Transform.GetLocation()); }
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		if ((Index == 0 && !Settings->bBlendFirstPoint) ||
			(Index == MaxIndex && !Settings->bBlendLastPoint))
		{
			return;
		}

		double Alpha = 0.5;
		const PCGExData::FPointRef Current = PointDataFacade->Source->GetOutPointRef(Index);
		MetadataBlender->PrepareForBlending(Current);

		if (Settings->BlendOver == EPCGExBlendOver::Distance)
		{
			Alpha = Length[Index] / Metrics.Length;
		}
		else if (Settings->BlendOver == EPCGExBlendOver::Index)
		{
			Alpha = static_cast<double>(Index) / static_cast<double>(PointDataFacade->GetNum());
		}
		else
		{
			Alpha = LerpCache->Read(Index);
		}

		MetadataBlender->Blend(*Start, *End, Current, Alpha);
		MetadataBlender->CompleteBlending(Current, 2, 1);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
