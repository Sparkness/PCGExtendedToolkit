﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGEx.h"
#include "Data/PCGExData.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "PCGExSampling.generated.h"

// Declaration & use pair, boolean will be set by name validation
#define PCGEX_OUTPUT_DECL_TOGGLE(_NAME, _TYPE, _DEFAULT_VALUE) bool bWrite##_NAME = false;
#define PCGEX_OUTPUT_DECL(_NAME, _TYPE, _DEFAULT_VALUE) TSharedPtr<PCGExData::TBuffer<_TYPE>> _NAME##Writer;
#define PCGEX_OUTPUT_DECL_AND_TOGGLE(_NAME, _TYPE, _DEFAULT_VALUE) PCGEX_OUTPUT_DECL_TOGGLE(_NAME, _TYPE, _DEFAULT_VALUE) PCGEX_OUTPUT_DECL(_NAME, _TYPE, _DEFAULT_VALUE)

// Simply validate name from settings
#define PCGEX_OUTPUT_VALIDATE_NAME(_NAME, _TYPE, _DEFAULT_VALUE)\
Context->bWrite##_NAME = Settings->bWrite##_NAME; \
if(Context->bWrite##_NAME && !FPCGMetadataAttributeBase::IsValidName(Settings->_NAME##AttributeName))\
{ PCGE_LOG(Warning, GraphAndLog, FTEXT("Invalid output attribute name for " #_NAME )); Context->bWrite##_NAME = false; }

#define PCGEX_OUTPUT_INIT(_NAME, _TYPE, _DEFAULT_VALUE) if(Context->bWrite##_NAME){ _NAME##Writer = OutputFacade->GetWritable<_TYPE>(Settings->_NAME##AttributeName, _DEFAULT_VALUE, true, PCGExData::EBufferInit::Inherit); }
#define PCGEX_OUTPUT_VALUE(_NAME, _INDEX, _VALUE) if(_NAME##Writer){_NAME##Writer->GetMutable(_INDEX) = _VALUE; }

UENUM()
enum class EPCGExSurfaceSource : uint8
{
	All             = 0 UMETA(DisplayName = "Any surface", ToolTip="Any surface within range will be tested"),
	ActorReferences = 1 UMETA(DisplayName = "Actor Reference", ToolTip="Only a list of actor surfaces will be included."),
};


UENUM()
enum class EPCGExSampleMethod : uint8
{
	WithinRange    = 0 UMETA(DisplayName = "All (Within range)", ToolTip="Use RangeMax = 0 to include all targets"),
	ClosestTarget  = 1 UMETA(DisplayName = "Closest Target", ToolTip="Picks & process the closest target only"),
	FarthestTarget = 2 UMETA(DisplayName = "Farthest Target", ToolTip="Picks & process the farthest target only"),
	BestCandidate  = 3 UMETA(DisplayName = "Best Candidate", ToolTip="Picks & process the best candidate based on sorting rules"),
};

UENUM()
enum class EPCGExSampleSource : uint8
{
	Source   = 0 UMETA(DisplayName = "Source", ToolTip="Read value on source"),
	Target   = 1 UMETA(DisplayName = "Target", ToolTip="Read value on target"),
	Constant = 2 UMETA(DisplayName = "Constant", ToolTip="Read constant"),
};

UENUM()
enum class EPCGExAngleRange : uint8
{
	URadians               = 0 UMETA(DisplayName = "Radians (0..+PI)", ToolTip="0..+PI"),
	PIRadians              = 1 UMETA(DisplayName = "Radians (-PI..+PI)", ToolTip="-PI..+PI"),
	TAURadians             = 2 UMETA(DisplayName = "Radians (0..+TAU)", ToolTip="0..TAU"),
	UDegrees               = 3 UMETA(DisplayName = "Degrees (0..+180)", ToolTip="0..+180"),
	PIDegrees              = 4 UMETA(DisplayName = "Degrees (-180..+180)", ToolTip="-180..+180"),
	TAUDegrees             = 5 UMETA(DisplayName = "Degrees (0..+360)", ToolTip="0..+360"),
	NormalizedHalf         = 6 UMETA(DisplayName = "Normalized Half (0..180 -> 0..1)", ToolTip="0..180 -> 0..1"),
	Normalized             = 7 UMETA(DisplayName = "Normalized (0..+360 -> 0..1)", ToolTip="0..+360 -> 0..1"),
	InvertedNormalizedHalf = 8 UMETA(DisplayName = "Inv. Normalized Half (0..180 -> 1..0)", ToolTip="0..180 -> 1..0"),
	InvertedNormalized     = 9 UMETA(DisplayName = "Inv. Normalized (0..+360 -> 1..0)", ToolTip="0..+360 -> 1..0"),
};

UENUM()
enum class EPCGExSampleWeightMode : uint8
{
	Distance      = 0 UMETA(DisplayName = "Distance", ToolTip="Weight is computed using distance to targets"),
	Attribute     = 1 UMETA(DisplayName = "Attribute", ToolTip="Uses a fixed attribute value on the target as weight"),
	AttributeMult = 2 UMETA(DisplayName = "Att x Dist", ToolTip="Uses a fixed attribute value on the target as a multiplier to distance-based weight"),
};

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Component Flags"))
enum class EPCGExApplySampledComponentFlags : uint8
{
	None = 0,
	X    = 1 << 0 UMETA(DisplayName = "X", ToolTip="Apply X Component"),
	Y    = 1 << 1 UMETA(DisplayName = "Y", ToolTip="Apply Y Component"),
	Z    = 1 << 2 UMETA(DisplayName = "Z", ToolTip="Apply Z Component"),
	All  = X | Y | Z UMETA(DisplayName = "All", ToolTip="Apply all Component"),
};

ENUM_CLASS_FLAGS(EPCGExApplySampledComponentFlags)
using EPCGExApplySampledComponentFlagsBitmask = TEnumAsByte<EPCGExApplySampledComponentFlags>;

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExApplySamplingDetails
{
	GENERATED_BODY()

	FPCGExApplySamplingDetails()
	{
	}


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bApplyTransform = false;

	/** Which position components from the sampled transform should be applied to the point.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" └─ Position", EditCondition="bApplyTransform", EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExApplySampledComponentFlags"))
	uint8 TransformPosition = 0;

	/** Which rotation components from the sampled transform should be applied to the point.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" └─ Rotation", EditCondition="bApplyTransform", EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExApplySampledComponentFlags"))
	uint8 TransformRotation = 0;

	/** Which scale components from the sampled transform should be applied to the point.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" └─ Scale", EditCondition="bApplyTransform", EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExApplySampledComponentFlags"))
	uint8 TransformScale = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bApplyLookAt = false;

	/** Which position components from the sampled look at should be applied to the point.  */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" └─ Position", EditCondition="bApplyLookAt", EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExApplySampledComponentFlags"))
	uint8 LookAtPosition = 0;

	/** Which rotation components from the sampled look at should be applied to the point.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" └─ Rotation", EditCondition="bApplyLookAt", EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExApplySampledComponentFlags"))
	uint8 LookAtRotation = 0;

	/** Which scale components from the sampled look at should be applied to the point.  */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" └─ Scale", EditCondition="bApplyLookAt", EditConditionHides, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExApplySampledComponentFlags"))
	uint8 LookAtScale = 0;

	int32 AppliedComponents = 0;
	TArray<int32> TrPosComponents;
	TArray<int32> TrRotComponents;
	TArray<int32> TrScaComponents;
	//TArray<int32> LkPosComponents;
	TArray<int32> LkRotComponents;
	//TArray<int32> LkScaComponents;

	bool WantsApply() const { return AppliedComponents > 0; }

	void Init()
	{
#define PCGEX_REGISTER_FLAG(_COMPONENT, _ARRAY) \
		if ((_COMPONENT & static_cast<uint8>(EPCGExApplySampledComponentFlags::X)) != 0){ _ARRAY.Add(0); AppliedComponents++; } \
		if ((_COMPONENT & static_cast<uint8>(EPCGExApplySampledComponentFlags::Y)) != 0){ _ARRAY.Add(1); AppliedComponents++; } \
		if ((_COMPONENT & static_cast<uint8>(EPCGExApplySampledComponentFlags::Z)) != 0){ _ARRAY.Add(2); AppliedComponents++; }

		if (bApplyTransform)
		{
			PCGEX_REGISTER_FLAG(TransformPosition, TrPosComponents)
			PCGEX_REGISTER_FLAG(TransformRotation, TrRotComponents)
			PCGEX_REGISTER_FLAG(TransformScale, TrScaComponents)
		}

		if (bApplyLookAt)
		{
			//PCGEX_REGISTER_FLAG(LookAtPosition, LkPosComponents)
			PCGEX_REGISTER_FLAG(LookAtRotation, LkRotComponents)
			//PCGEX_REGISTER_FLAG(LookAtScale, LkScaComponents)
		}

#undef PCGEX_REGISTER_FLAG
	}

	void Apply(FPCGPoint& InPoint, const FTransform& InTransform, const FTransform& InLookAt)
	{
		FVector OutRotation = InPoint.Transform.GetRotation().Euler();
		FVector OutPosition = InPoint.Transform.GetLocation();
		FVector OutScale = InPoint.Transform.GetScale3D();

		if (bApplyTransform)
		{
			const FVector InTrRot = InTransform.GetRotation().Euler();
			for (const int32 C : TrRotComponents) { OutRotation[C] = InTrRot[C]; }

			FVector InTrPos = InTransform.GetLocation();
			for (const int32 C : TrPosComponents) { OutPosition[C] = InTrPos[C]; }

			FVector InTrSca = InTransform.GetScale3D();
			for (const int32 C : TrScaComponents) { OutScale[C] = InTrSca[C]; }
		}

		if (bApplyLookAt)
		{
			const FVector InLkRot = InLookAt.GetRotation().Euler();
			for (const int32 C : LkRotComponents) { OutRotation[C] = InLkRot[C]; }

			//FVector InLkPos = InLookAt.GetLocation();
			//for (const int32 C : LkPosComponents) { OutPosition[C] = InLkPos[C]; }

			//FVector InLkSca = InLookAt.GetScale3D();
			//for (const int32 C : LkScaComponents) { OutScale[C] = InLkSca[C]; }
		}


		InPoint.Transform = FTransform(FQuat::MakeFromEuler(OutRotation), OutPosition, OutScale);
	}
};

namespace PCGExSampling
{
	const FName SourceIgnoreActorsLabel = TEXT("InIgnoreActors");
	const FName SourceActorReferencesLabel = TEXT("ActorReferences");
	const FName OutputSampledActorsLabel = TEXT("OutSampledActors");

	FORCEINLINE static double GetAngle(const EPCGExAngleRange Mode, const FVector& A, const FVector& B)
	{
		double OutAngle = 0;

		const FVector N1 = A.GetSafeNormal();
		const FVector N2 = B.GetSafeNormal();

		const double MainDot = N1.Dot(N2);
		const FVector C = FVector::CrossProduct(N1, N2);

		switch (Mode)
		{
		case EPCGExAngleRange::URadians: // 0 .. 3.14
			OutAngle = FMath::Acos(MainDot);
			break;
		case EPCGExAngleRange::PIRadians: // -3.14 .. 3.14
			OutAngle = FMath::Acos(MainDot) * FMath::Sign(MainDot);
			break;
		case EPCGExAngleRange::TAURadians: // 0 .. 6.28
			if (C.Z < 0) { OutAngle = TWO_PI - FMath::Atan2(C.Size(), MainDot); }
			else { OutAngle = FMath::Atan2(C.Size(), MainDot); }
			break;
		case EPCGExAngleRange::UDegrees: // 0 .. 180
			OutAngle = FMath::RadiansToDegrees(FMath::Acos(MainDot));
			break;
		case EPCGExAngleRange::PIDegrees: // -180 .. 180
			OutAngle = FMath::RadiansToDegrees(FMath::Acos(MainDot)) * FMath::Sign(MainDot);
			break;
		case EPCGExAngleRange::TAUDegrees: // 0 .. 360
			if (C.Z < 0) { OutAngle = 360 - FMath::RadiansToDegrees(FMath::Atan2(C.Size(), MainDot)); }
			else { OutAngle = FMath::RadiansToDegrees(FMath::Atan2(C.Size(), MainDot)); }
			break;
		case EPCGExAngleRange::NormalizedHalf: // 0 .. 180 -> 0 .. 1
			OutAngle = FMath::RadiansToDegrees(FMath::Acos(MainDot)) / 180;
			break;
		case EPCGExAngleRange::Normalized: // 0 .. 360 -> 0 .. 1
			if (C.Z < 0) { OutAngle = 360 - FMath::RadiansToDegrees(FMath::Atan2(C.Size(), MainDot)); }
			else { OutAngle = FMath::RadiansToDegrees(FMath::Atan2(C.Size(), MainDot)); }
			OutAngle /= 360;
			break;
		case EPCGExAngleRange::InvertedNormalizedHalf: // 0 .. 180 -> 1 .. 0
			OutAngle = 1 - (FMath::RadiansToDegrees(FMath::Acos(MainDot)) / 180);
			break;
		case EPCGExAngleRange::InvertedNormalized: // 0 .. 360 -> 1 .. 0
			if (C.Z < 0) { OutAngle = 360 - FMath::RadiansToDegrees(FMath::Atan2(C.Size(), MainDot)); }
			else { OutAngle = FMath::RadiansToDegrees(FMath::Atan2(C.Size(), MainDot)); }
			OutAngle /= 360;
			OutAngle = 1 - OutAngle;
			break;
		default: ;
		}

		return OutAngle;
	}

	static bool GetIncludedActors(
		const FPCGContext* InContext,
		const TSharedRef<PCGExData::FFacade>& InFacade,
		const FName ActorReferenceName,
		TMap<AActor*, int32>& OutActorSet)
	{
		FPCGAttributePropertyInputSelector Selector = FPCGAttributePropertyInputSelector();
		Selector.SetAttributeName(ActorReferenceName);

		const TUniquePtr<PCGEx::TAttributeBroadcaster<FSoftObjectPath>> ActorReferences = MakeUnique<PCGEx::TAttributeBroadcaster<FSoftObjectPath>>();
		if (!ActorReferences->Prepare(Selector, InFacade->Source))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Actor reference attribute does not exist."));
			return false;
		}

		ActorReferences->Grab();

		for (int i = 0; i < ActorReferences->Values.Num(); i++)
		{
			const FSoftObjectPath& Path = ActorReferences->Values[i];
			if (!Path.IsValid()) { continue; }
			if (AActor* TargetActor = Cast<AActor>(Path.ResolveObject())) { OutActorSet.FindOrAdd(TargetActor, i); }
		}

		return true;
	}

	static void PruneFailedSamples(TArray<FPCGPoint>& InMutablePoints, const TArray<int8>& InSampleState)
	{
		const int32 NumSamples = InMutablePoints.Num();

		check(InMutablePoints.Num() == NumSamples);

		int32 WriteIndex = 0;
		for (int32 i = 0; i < NumSamples; i++) { if (InSampleState[i]) { InMutablePoints[WriteIndex++] = InMutablePoints[i]; } }
		InMutablePoints.SetNum(WriteIndex);
	}
}
