﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"

#include "PCGExProbeFactoryProvider.generated.h"

#define PCGEX_CREATE_PROBE_FACTORY(_NAME, _EXTRA_FACTORY, _EXTRA_OPERATION) \
UPCGExFactoryData* UPCGExProbe##_NAME##ProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const{\
	UPCGExProbeFactory##_NAME* NewFactory = InContext->ManagedObjects->New<UPCGExProbeFactory##_NAME>();\
	NewFactory->Config = Config; _EXTRA_FACTORY \
	return Super::CreateFactory(InContext, NewFactory); } \
UPCGExProbeOperation* UPCGExProbeFactory##_NAME::CreateOperation(FPCGExContext* InContext) const{\
	UPCGExProbe##_NAME* NewOperation = InContext->ManagedObjects->New<UPCGExProbe##_NAME>(this->GetOuter());\
	NewOperation->Config = Config; NewOperation->BaseConfig = &NewOperation->Config; _EXTRA_OPERATION return NewOperation;}

class UPCGExProbeOperation;

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExProbeFactoryData : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::Probe; }
	virtual UPCGExProbeOperation* CreateOperation(FPCGExContext* InContext) const;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExProbeFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(AbstractProbe, "Probe Definition", "Creates a single probe to look for a nearby connection.")
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorProbe; }
#endif
	//~End UPCGSettings

	virtual FName GetMainOutputPin() const override { return PCGExGraph::OutputProbeLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

protected:
	virtual bool IsCacheable() const override { return true; }
};
