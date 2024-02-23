﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Geometry/PCGExGeo.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExFindContours.generated.h"

UENUM(BlueprintType)
enum class EPCGExContourOrientation : uint8
{
	Left UMETA(DisplayName = "Lefty", ToolTip="Selects first lefty edge"),
	Right UMETA(DisplayName = "Righty", ToolTip="Selects first righty edge"),
};

namespace PCGExContours
{
	struct PCGEXTENDEDTOOLKIT_API FCandidate
	{
		explicit FCandidate(const int32 InNodeIndex)
			: NodeIndex(InNodeIndex)
		{
		}

		int32 NodeIndex;
		double Distance = TNumericLimits<double>::Max();
		double Dot = -1;
	};
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExFindContoursSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExFindContoursSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindContours, "Edges : Find Contours", "Attempts to find a closed contour of connected edges around seed points.");
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

	/** Drives how the seed nodes are selected within the graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExClusterClosestSearchMode NodePickingMode = EPCGExClusterClosestSearchMode::Node;
	
	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionSettings ProjectionSettings;

private:
	friend class FPCGExFindContoursElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFindContoursContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExFindContoursElement;
	friend class FPCGExCreateBridgeTask;

	virtual ~FPCGExFindContoursContext() override;

	PCGExData::FPointIOCollection* Seeds;
	PCGExData::FPointIOCollection* Paths;

	TArray<TArray<FVector>*> ProjectedSeeds;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFindContoursElement : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFindContourTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExFindContourTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                      PCGExCluster::FCluster* InCluster) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		Cluster(InCluster)
	{
	}

	PCGExCluster::FCluster* Cluster = nullptr;

	virtual bool ExecuteTask() override;
};
