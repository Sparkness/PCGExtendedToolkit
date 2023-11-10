﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "PCGPin.h"
#include "Data/PCGExRelationsParamsData.h"
#include "PCGExRelationsParamsBuilder.generated.h"

/** Outputs a single RelationalParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExRelationsParamsBuilderSettings : public UPCGSettings
{
	GENERATED_BODY()

	UPCGExRelationsParamsBuilderSettings(const FObjectInitializer& ObjectInitializer);

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("RelationalParams")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExRelationalParams", "NodeTitle", "Relational Params"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Attribute name to store relation data to. Note that since it uses a custom data type, it won't show up in editor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName RelationIdentifier = "RelationIdentifier";

	/** Whether to mark mutual relations. Additional performance cost. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bMarkMutualRelations = true;

	/** Attribute name to store relation data to. Note that since it uses a custom data type, it won't show up in editor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(TitleProperty="{AttributeName}"))
	TArray<FPCGExSocketDescriptor> Sockets;

protected:
	virtual void InitDefaultSockets();
};

class PCGEXTENDEDTOOLKIT_API FPCGExRelationsParamsBuilderElement : public FSimplePCGElement
{
protected:
	template <typename T>
	T* BuildParams(FPCGContext* Context) const;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
