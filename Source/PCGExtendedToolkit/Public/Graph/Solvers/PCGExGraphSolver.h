﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExGraphProcessor.h"
#include "UObject/Object.h"
#include "PCGExGraphSolver.generated.h"

namespace PCGExGraph
{
	struct PCGEXTENDEDTOOLKIT_API FPointCandidate
	{
		FPointCandidate()
		{
		}

		double Distance = TNumericLimits<double>::Max();
		double Dot = -1;

		int32 Index = -1;
		PCGMetadataEntryKey EntryKey = PCGInvalidEntryKey;
	};

	/** Per-socket temp data structure for processing only*/
	struct PCGEXTENDEDTOOLKIT_API FSocketProbe : FPCGExSocketAngle
	{
		FSocketProbe()
		{
			Candidates.Empty();
		}

	public:
		FSocketInfos* SocketInfos = nullptr;
		FVector Origin = FVector::Zero();

		TArray<FPointCandidate> Candidates;
		FPointCandidate BestCandidate;

		FBox LooseBounds;

		double IndexedRating = TNumericLimits<double>::Max();
		double IndexedDistanceRating = 0;
		double IndexedDotRating = 0;
		double IndexedDotWeight = 0;

		double ProbedDistanceMax = 0;
		double ProbedDistanceMin = TNumericLimits<double>::Max();
		double ProbedDotMax = 0;
		double ProbedDotMin = TNumericLimits<double>::Max();

		void OutputTo(PCGMetadataEntryKey Key) const
		{
			SocketInfos->Socket->SetTargetIndex(Key, BestCandidate.Index);
			SocketInfos->Socket->SetTargetEntryKey(Key, BestCandidate.EntryKey);
		}

		~FSocketProbe()
		{
			SocketInfos = nullptr;
		}
	};
}

/**
 * 
 */
UCLASS(Blueprintable, EditInlineNew)
class PCGEXTENDEDTOOLKIT_API UPCGExGraphSolver : public UObject
{
	GENERATED_BODY()

public:
	virtual void InitializeProbe(PCGExGraph::FSocketProbe& Probe) const;
	virtual bool ProcessPoint(PCGExGraph::FSocketProbe& Probe, const FPCGPoint& Point, const int32 Index) const;
	virtual void ResolveProbe(PCGExGraph::FSocketProbe& Probe) const;

	virtual double PrepareProbesForPoint(const TArray<PCGExGraph::FSocketInfos>& SocketInfos, const FPCGPoint& Point, TArray<PCGExGraph::FSocketProbe>& OutProbes) const;
	virtual double PrepareProbeForPointSocketPair(const FPCGPoint& Point, PCGExGraph::FSocketProbe& Probe, const PCGExGraph::FSocketInfos& InSocketInfos) const;
};
