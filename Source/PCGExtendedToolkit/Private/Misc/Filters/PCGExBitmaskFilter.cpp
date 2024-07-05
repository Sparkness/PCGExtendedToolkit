﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExBitmaskFilter.h"

PCGExPointFilter::TFilter* UPCGExBitmaskFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TBitmaskFilter(this);
}

bool PCGExPointsFilter::TBitmaskFilter::Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
{
	if (!TFilter::Init(InContext, InPointDataFacade)) { return false; }

	FlagsReader = PointDataFacade->GetOrCreateReader<int64>(TypedFilterFactory->Descriptor.FlagsAttribute);

	if (!FlagsReader)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Value attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.FlagsAttribute)));
		return false;
	}

	if (TypedFilterFactory->Descriptor.MaskType == EPCGExFetchType::Attribute)
	{
		MaskReader = PointDataFacade->GetOrCreateReader<int64>(TypedFilterFactory->Descriptor.BitmaskAttribute);
		if (!MaskReader)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Mask attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.BitmaskAttribute)));
			return false;
		}
	}

	return true;
}

bool PCGExPointsFilter::TBitmaskFilter::Test(const int32 PointIndex) const
{
	const bool Result = PCGExCompare::Compare(
		TypedFilterFactory->Descriptor.Comparison,
		FlagsReader->Values[PointIndex],
		MaskReader ? MaskReader->Values[PointIndex] : Bitmask);

	return TypedFilterFactory->Descriptor.bInvertResult ? !Result : Result;
}

namespace PCGExCompareFilter
{
}

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

PCGEX_CREATE_FILTER_FACTORY(Bitmask)

#if WITH_EDITOR
FString UPCGExBitmaskFilterProviderSettings::GetDisplayName() const
{
	FString A = Descriptor.MaskType == EPCGExFetchType::Attribute ? Descriptor.BitmaskAttribute.ToString() : TEXT("(Const)");
	FString B = Descriptor.FlagsAttribute.ToString();
	FString DisplayName;

	switch (Descriptor.Comparison)
	{
	case EPCGExBitflagComparison::MatchPartial:
		DisplayName = TEXT("Contains Any");
	//DisplayName = TEXT("A & B != 0");
	//DisplayName = A + " & " + B + TEXT(" != 0");
		break;
	case EPCGExBitflagComparison::MatchFull:
		DisplayName = TEXT("Contains All");
	//DisplayName = TEXT("A & B == B");
	//DisplayName = A + " Any " + B + TEXT(" == B");
		break;
	case EPCGExBitflagComparison::MatchStrict:
		DisplayName = TEXT("Is Exactly");
	//DisplayName = TEXT("A == B");
	//DisplayName = A + " == " + B;
		break;
	case EPCGExBitflagComparison::NoMatchPartial:
		DisplayName = TEXT("Not Contains Any");
	//DisplayName = TEXT("A & B == 0");
	//DisplayName = A + " & " + B + TEXT(" == 0");
		break;
	case EPCGExBitflagComparison::NoMatchFull:
		DisplayName = TEXT("Not Contains All");
	//DisplayName = TEXT("A & B != B");
	//DisplayName = A + " & " + B + TEXT(" != B");
		break;
	default:
		DisplayName = " ?? ";
		break;
	}

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
