﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExBroadcast.h"
#include "Data/PCGExData.h"

namespace PCGEx
{
	bool GetComponentSelection(const TArray<FString>& Names, FInputSelectorComponentData& OutSelection)
	{
		if (Names.IsEmpty()) { return false; }
		for (const FString& Name : Names)
		{
			if (const FInputSelectorComponentData* Selection = STRMAP_TRANSFORM_FIELD.Find(Name.ToUpper()))
			{
				OutSelection = *Selection;
				return true;
			}
		}
		return false;
	}

	bool GetFieldSelection(const TArray<FString>& Names, FInputSelectorFieldData& OutSelection)
	{
		if (Names.IsEmpty()) { return false; }
		const FString& STR = Names.Num() > 1 ? Names[1].ToUpper() : Names[0].ToUpper();
		if (const FInputSelectorFieldData* Selection = STRMAP_SINGLE_FIELD.Find(STR))
		{
			OutSelection = *Selection;
			return true;
		}
		return false;
	}

	bool GetAxisSelection(const TArray<FString>& Names, FInputSelectorAxisData& OutSelection)
	{
		if (Names.IsEmpty()) { return false; }
		for (const FString& Name : Names)
		{
			if (const FInputSelectorAxisData* Selection = STRMAP_AXIS.Find(Name.ToUpper()))
			{
				OutSelection = *Selection;
				return true;
			}
		}
		return false;
	}

	FSubSelection::FSubSelection(const TArray<FString>& ExtraNames)
	{
		Init(ExtraNames);
	}

	FSubSelection::FSubSelection(const FPCGAttributePropertyInputSelector& InSelector)
	{
		Init(InSelector.GetExtraNames());
	}

	FSubSelection::FSubSelection(const FString& Path, const UPCGData* InData)
	{
		FPCGAttributePropertyInputSelector ProxySelector = FPCGAttributePropertyInputSelector();
		ProxySelector.Update(Path);
		if (InData) { ProxySelector = ProxySelector.CopyAndFixLast(InData); }

		Init(ProxySelector.GetExtraNames());
	}

	EPCGMetadataTypes FSubSelection::GetSubType(const EPCGMetadataTypes Fallback) const
	{
		if (!bIsValid) { return Fallback; }
		if (bIsFieldSet) { return EPCGMetadataTypes::Double; }
		if (bIsAxisSet) { return EPCGMetadataTypes::Vector; }

		switch (Component)
		{
		case EPCGExTransformComponent::Position:
		case EPCGExTransformComponent::Scale:
			return EPCGMetadataTypes::Vector;
		case EPCGExTransformComponent::Rotation:
			return EPCGMetadataTypes::Quaternion;
		}

		return Fallback;
	}

	void FSubSelection::Init(const TArray<FString>& ExtraNames)
	{
		if (ExtraNames.IsEmpty())
		{
			bIsValid = false;
			return;
		}

		FInputSelectorAxisData AxisIDMapping = FInputSelectorAxisData{EPCGExAxis::Forward, EPCGMetadataTypes::Unknown};
		FInputSelectorComponentData ComponentIDMapping = {EPCGExTransformComponent::Rotation, EPCGMetadataTypes::Quaternion};
		FInputSelectorFieldData FieldIDMapping = {EPCGExSingleField::X, EPCGMetadataTypes::Unknown, 0};

		bIsAxisSet = GetAxisSelection(ExtraNames, AxisIDMapping);
		Axis = AxisIDMapping.Get<0>();

		bIsComponentSet = GetComponentSelection(ExtraNames, ComponentIDMapping);

		if (bIsAxisSet)
		{
			bIsValid = true;
			bIsComponentSet = GetComponentSelection(ExtraNames, ComponentIDMapping);
		}
		else
		{
			bIsValid = bIsComponentSet;
		}

		Component = ComponentIDMapping.Get<0>();
		PossibleSourceType = ComponentIDMapping.Get<1>();

		bIsFieldSet = GetFieldSelection(ExtraNames, FieldIDMapping);
		Field = FieldIDMapping.Get<0>();
		FieldIndex = FieldIDMapping.Get<2>();

		if (bIsFieldSet)
		{
			bIsValid = true;
			if (!bIsComponentSet) { PossibleSourceType = FieldIDMapping.Get<1>(); }
		}

		Update();
	}

	void FSubSelection::Update()
	{
		switch (Field)
		{
		case EPCGExSingleField::X:
			FieldIndex = 0;
			break;
		case EPCGExSingleField::Y:
			FieldIndex = 1;
			break;
		case EPCGExSingleField::Z:
			FieldIndex = 2;
			break;
		case EPCGExSingleField::W:
			FieldIndex = 3;
			break;
		case EPCGExSingleField::Length:
		case EPCGExSingleField::SquaredLength:
		case EPCGExSingleField::Volume:
		case EPCGExSingleField::Sum:
			FieldIndex = 0;
			break;
		}
	}

	bool TryGetTypeAndSource(
		const FPCGAttributePropertyInputSelector& InputSelector,
		const TSharedPtr<PCGExData::FFacade>& InDataFacade,
		EPCGMetadataTypes& OutType, PCGExData::ESource& InOutSource)
	{
		OutType = EPCGMetadataTypes::Unknown;
		const UPCGPointData* Data = InOutSource == PCGExData::ESource::In ? InDataFacade->Source->GetInOut() : InDataFacade->Source->GetOutIn();
		if (!InDataFacade->Source->GetSource(Data, InOutSource)) { return false; }

		const FPCGAttributePropertyInputSelector FixedSelector = InputSelector.CopyAndFixLast(Data);
		if (!FixedSelector.IsValid()) { return false; }

		const TArray<FString>& ExtraNames = FixedSelector.GetExtraNames();

		if (FixedSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			if (!Data->Metadata) { return false; }
			if (const FPCGMetadataAttributeBase* AttributeBase = Data->Metadata->GetConstAttribute(FixedSelector.GetName()))
			{
				OutType = static_cast<EPCGMetadataTypes>(AttributeBase->GetTypeId());
			}
		}
		else if (FixedSelector.GetSelection() == EPCGAttributePropertySelection::ExtraProperty)
		{
			OutType = GetPropertyType(FixedSelector.GetExtraProperty());
		}
		else if (FixedSelector.GetSelection() == EPCGAttributePropertySelection::PointProperty)
		{
			OutType = GetPropertyType(FixedSelector.GetPointProperty());
		}

		return OutType != EPCGMetadataTypes::Unknown;
	}

	bool TryGetTypeAndSource(
		const FName AttributeName,
		const TSharedPtr<PCGExData::FFacade>& InDataFacade,
		EPCGMetadataTypes& OutType, PCGExData::ESource& InOutSource)
	{
		return true;
	}
}
