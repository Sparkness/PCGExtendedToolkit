﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExMT.h"
#include "UObject/Object.h"

#include "Data/PCGExAttributeHelpers.h"

class PCGEXTENDEDTOOLKIT_API FPCGExPointIOMerger final
{
	friend class FPCGExAttributeMergeTask;

public:
	TArray<PCGEx::FAttributeIdentity> UniqueIdentities;
	PCGExData::FPointIO* CompositeIO = nullptr;
	TArray<PCGExData::FPointIO*> IOSources;
	TArray<uint64> Scopes;
	TArray<PCGEx::FAAttributeIO*> Writers;

	FPCGExPointIOMerger(PCGExData::FPointIO& OutData);
	~FPCGExPointIOMerger();

	void Append(PCGExData::FPointIO* InData);
	void Append(const TArray<PCGExData::FPointIO*>& InData);
	void Append(PCGExData::FPointIOCollection* InCollection);
	void Merge(PCGExMT::FTaskManager* AsyncManager, const TSet<FName>* IgnoreAttributeSet = nullptr);
	void Write();
	void Write(PCGExMT::FTaskManager* AsyncManager);

protected:
	int32 NumCompositePoints = 0;
};

namespace PCGExPointIOMerger
{
	class PCGEXTENDEDTOOLKIT_API FWriteAttributeTask final : public PCGExMT::FPCGExTask
	{
	public:
		FWriteAttributeTask(
			PCGExData::FPointIO* InPointIO,
			FPCGExPointIOMerger* InMerger)
			: FPCGExTask(InPointIO),
			  Merger(InMerger)
		{
		}

		FPCGExPointIOMerger* Merger = nullptr;
		virtual bool ExecuteTask() override;
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FWriteAttributeScopeTask final : public PCGExMT::FPCGExTask
	{
	public:
		FWriteAttributeScopeTask(
			PCGExData::FPointIO* InPointIO,
			const uint64 InScope,
			const PCGEx::FAttributeIdentity& InIdentity,
			PCGEx::TFAttributeWriter<T>* InWriter)
			: FPCGExTask(InPointIO),
			  Scope(InScope),
			  Identity(InIdentity),
			  Writer(InWriter)
		{
		}

		const uint64 Scope;
		const PCGEx::FAttributeIdentity Identity;
		PCGEx::TFAttributeWriter<T>* Writer = nullptr;

		virtual bool ExecuteTask() override
		{
			PCGEx::TFAttributeReader<T>* Reader = new PCGEx::TFAttributeReader<T>(Identity.Name);
			Reader->Bind(PointIO);

			uint32 StartIndex;
			uint32 Range;
			PCGEx::H64(Scope, StartIndex, Range);

			int32 Count = static_cast<int>(Range);
			for (int i = 0; i < Count; i++) { Writer->Values[StartIndex + i] = Reader->Values[i]; }

			PCGEX_DELETE(Reader);
			return true;
		}
	};
}
