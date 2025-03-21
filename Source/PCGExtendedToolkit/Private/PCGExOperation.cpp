﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExOperation.h"
#include "PCGParamData.h"

void UPCGExOperation::BindContext(FPCGExContext* InContext)
{
	Context = InContext;
}

void UPCGExOperation::FindSettingsOverrides(FPCGExContext* InContext, FName InPinLabel)
{
	TArray<FPCGTaggedData> OverrideParams = Context->InputData.GetParamsByPin(InPinLabel);
	for (FPCGTaggedData& InTaggedData : OverrideParams)
	{
		const UPCGParamData* ParamData = Cast<UPCGParamData>(InTaggedData.Data);

		if (!ParamData) { continue; }
		const TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(ParamData->Metadata);

		for (PCGEx::FAttributeIdentity& Identity : Infos->Identities)
		{
			PossibleOverrides.Add(Identity.Name, ParamData->Metadata->GetMutableAttribute(Identity.Name));
		}
	}

	ApplyOverrides();
	PossibleOverrides.Empty();
}

#if WITH_EDITOR
void UPCGExOperation::UpdateUserFacingInfos()
{
}
#endif

void UPCGExOperation::Cleanup()
{
	Context = nullptr;
	PrimaryDataFacade.Reset();
	SecondaryDataFacade.Reset();
}

void UPCGExOperation::RegisterConsumableAttributesWithFacade(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InFacade) const
{
}

void UPCGExOperation::RegisterPrimaryBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) const
{
}

void UPCGExOperation::BeginDestroy()
{
	Cleanup();
	UObject::BeginDestroy();
}

void UPCGExOperation::ApplyOverrides()
{
	UClass* ObjectClass = GetClass();

	for (const TPair<FName, FPCGMetadataAttributeBase*>& PossibleOverride : PossibleOverrides)
	{
		// Find the property by name
		FProperty* Property = ObjectClass->FindPropertyByName(PossibleOverride.Key);
		if (!Property) { continue; }

		PCGEx::ExecuteWithRightType(
			PossibleOverride.Value->GetTypeId(), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				const FPCGMetadataAttribute<T>* TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(PossibleOverride.Value);
				PCGEx::TrySetFPropertyValue<T>(this, Property, TypedAttribute->GetValue(0));
			});
	}
}

void UPCGExOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	BindContext(Other->Context);
	PCGExHelpers::CopyProperties(this, Other);
}

void UPCGExOperation::RegisterAssetDependencies(FPCGExContext* InContext)
{
	//InContext->AddAssetDependency();
}
