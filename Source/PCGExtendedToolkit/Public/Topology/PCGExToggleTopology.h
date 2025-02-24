﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGEx.h"
#include "PCGExPointsProcessor.h"

#include "PCGExToggleTopology.generated.h"

UENUM()
enum class EPCGExToggleTopologyAction : uint8
{
	Toggle = 0 UMETA(DisplayName = "Toggle", ToolTip="..."),
	Remove = 1 UMETA(DisplayName = "Remove", ToolTip="..."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGExToggleTopologySettings : public UPCGSettings
{
	GENERATED_BODY()

	friend class FPCGExToggleTopologyElement;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_DUMMY_SETTINGS_MEMBERS
	PCGEX_NODE_INFOS(ToggleTopology, "Topology : Toggle", "Registers/unregister or Removes PCGEx spawned dynamic meshes.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPrimitives; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExToggleTopologyAction Action = EPCGExToggleTopologyAction::Toggle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Action==EPCGExToggleTopologyAction::Toggle", EditConditionHides))
	bool bToggle = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bFilterByTag = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bFilterByTag", EditConditionHides))
	FName CommaSeparatedTagFilters = FName("PCGExTopology");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable), AdvancedDisplay)
	TSoftObjectPtr<AActor> TargetActor;
};

struct FPCGExToggleTopologyContext final : FPCGContext
{
	friend class FPCGExToggleTopologyElement;
	bool bWait = true;
};

class FPCGExToggleTopologyElement final : public IPCGElement
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	PCGEX_CAN_ONLY_EXECUTE_ON_MAIN_THREAD(true)

	//virtual void DisabledPassThroughData(FPCGContext* Context) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
