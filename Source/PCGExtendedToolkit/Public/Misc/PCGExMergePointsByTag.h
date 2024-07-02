﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExMergePointsByTag.generated.h"

class FPCGExPointIOMerger;

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Selector Type"))
enum class EPCGExMergeByTagOverlapResolutionMode : uint8
{
	Strict UMETA(DisplayName = "Strict", ToolTip="Merge happens per-tag, and higher priority tags are removed from lower priority overlaps."),
	ImmediateOverlap UMETA(DisplayName = "Overlap", ToolTip="Merge happens per-tag, overlapping data is merged entierely."),
};

namespace PCPGExMergePointsByTag
{
	class PCGEXTENDEDTOOLKIT_API FMergeList
	{
	public:
		TArray<PCGExData::FPointIO*> IOs;
		PCGExData::FPointIO* CompositeIO = nullptr;
		FPCGExPointIOMerger* Merger = nullptr;

		FMergeList();
		~FMergeList();

		void Merge(PCGExMT::FTaskManager* AsyncManager);
		void Write(PCGExMT::FTaskManager* AsyncManager);
	};

	class PCGEXTENDEDTOOLKIT_API FTagBucket
	{
	public:
		FString Tag;
		TArray<PCGExData::FPointIO*> IOs;
		explicit FTagBucket(const FString& InTag);
		~FTagBucket();
	};

	class PCGEXTENDEDTOOLKIT_API FTagBuckets
	{
	public:
		TArray<FTagBucket*> Buckets;
		TMap<FString, int32> BucketsMap;
		TMap<PCGExData::FPointIO*, TSet<FTagBucket*>*> ReverseBucketsMap;
		explicit FTagBuckets(PCGExData::FPointIOCollection* InCollection);
		~FTagBuckets();

		void Distribute(PCGExData::FPointIO* IO);
		void AddToReverseMap(PCGExData::FPointIO* IO, FTagBucket* Bucket);
		void BuildMergeLists(EPCGExMergeByTagOverlapResolutionMode Mode, TArray<FMergeList*>& OutLists, const TArray<FString>& Priorities);
	};
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExMergePointsByTagSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MergePointsByTag, "Merge Points by Tag", "Merge points based on shared tags.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExMergeByTagOverlapResolutionMode Mode = EPCGExMergeByTagOverlapResolutionMode::Strict;

	/** List of tags that should not be checked. This include prefixes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSet<FString> IgnoreTagsAndPrefixes = {PCGExGraph::TagStr_ClusterPair, PCGExGraph::TagStr_PCGExEdges, PCGExGraph::TagStr_PCGExVtx};

	/** Which tag has merging authority over another. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TArray<FString> ResolutionPriorities;

};

struct PCGEXTENDEDTOOLKIT_API FPCGExMergePointsByTagContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExMergePointsByTagElement;

	virtual ~FPCGExMergePointsByTagContext() override;

	TArray<PCPGExMergePointsByTag::FMergeList*> MergeLists;
};

class PCGEXTENDEDTOOLKIT_API FPCGExMergePointsByTagElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExMergePointsByTag
{
	
}
