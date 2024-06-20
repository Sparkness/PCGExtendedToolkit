// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExMT.h"
#include "PCGExOperation.h"
#include "Data/PCGExDataFilter.h"
#include "Graph/PCGExGraph.h"

namespace PCGExPointsMT
{
	enum class EMode : uint8
	{
		None     = 0,
		Process  = 1 << 0,
		Complete = 1 << 1,
		Write    = 1 << 2,
	};

	PCGEX_ASYNC_STATE(MTState_PointsProcessing)
	PCGEX_ASYNC_STATE(MTState_PointsCompletingWork)
	PCGEX_ASYNC_STATE(MTState_PointsWriting)

	PCGEX_ASYNC_STATE(State_PointsAsyncWorkComplete)

#pragma region Tasks

#define PCGEX_POINTS_MT_TASK(_NAME, _BODY)\
template <typename T>\
class PCGEXTENDEDTOOLKIT_API _NAME final : public FPCGExNonAbandonableTask	{\
public: _NAME(PCGExData::FPointIO* InPointIO, T* InTarget) : FPCGExNonAbandonableTask(InPointIO),Target(InTarget){} \
T* Target = nullptr; virtual bool ExecuteTask() override{_BODY return true; }};

#define PCGEX_POINTS_MT_TASK_RANGE(_NAME, _BODY)\
template <typename T>\
class PCGEXTENDEDTOOLKIT_API _NAME final : public FPCGExNonAbandonableTask	{\
public: _NAME(PCGExData::FPointIO* InPointIO, T* InTarget, const int32 InIterations, const PCGExData::ESource InSource = PCGExData::ESource::In) : FPCGExNonAbandonableTask(InPointIO),Target(InTarget), Iterations(InIterations), Source(InSource){} \
T* Target = nullptr; const int32 Iterations = 0; const PCGExData::ESource Source; virtual bool ExecuteTask() override{_BODY return true; }};

	PCGEX_POINTS_MT_TASK(FStartPointsBatchProcessing, { if (Target->PrepareProcessing()) { Target->Process(Manager); } })

	PCGEX_POINTS_MT_TASK(FAsyncProcess, { Target->Process(Manager); })

	PCGEX_POINTS_MT_TASK(FAsyncCompleteWork, { Target->CompleteWork(); })

	PCGEX_POINTS_MT_TASK(FAsyncWrite, { Target->Write(); })

	PCGEX_POINTS_MT_TASK_RANGE(FAsyncProcessPointRange, {Target->ProcessPoints(Source, TaskIndex, Iterations);})

	PCGEX_POINTS_MT_TASK_RANGE(FAsyncProcessRange, {Target->ProcessRange(TaskIndex, Iterations);})

	PCGEX_POINTS_MT_TASK_RANGE(FAsyncClosedBatchProcessRange, {Target->ProcessClosedBatchRange(TaskIndex, Iterations);})

#pragma endregion

	class FPointsProcessor
	{
	protected:
		FPCGExAsyncManager* AsyncManagerPtr = nullptr;

	public:
		TArray<UPCGExFilterFactoryBase*>* FilterFactories = nullptr;
		bool DefaultPointFilterValue = true;
		bool bIsSmallPoints = false;

		TArray<bool> PointFilterCache;

		FPCGContext* Context = nullptr;

		PCGExData::FPointIO* PointIO = nullptr;
		int32 BatchIndex = -1;

		UPCGExOperation* PrimaryOperation = nullptr;

		explicit FPointsProcessor(PCGExData::FPointIO* InPoints):
			PointIO(InPoints)
		{
		}

		virtual ~FPointsProcessor()
		{
			PointIO = nullptr;
			PCGEX_DELETE_OPERATION(PrimaryOperation)

			PointFilterCache.Empty();
		}

		template <typename T>
		T* GetContext() { return static_cast<T*>(Context); }

		bool IsTrivial() const { return bIsSmallPoints; }

		void SetPointsFilterData(TArray<UPCGExFilterFactoryBase*>* InFactories)
		{
			FilterFactories = InFactories;
		}

		virtual bool Process(FPCGExAsyncManager* AsyncManager)
		{
			AsyncManagerPtr = AsyncManager;

#pragma region Path filter data

			if (FilterFactories)
			{
				if (FilterFactories->IsEmpty())
				{
					PointFilterCache.SetNumUninitialized(PointIO->GetNum());
					for (int i = 0; i < PointIO->GetNum(); i++) { PointFilterCache[i] = DefaultPointFilterValue; }
				}
				else
				{
					PointFilterCache.Empty();

					PCGExDataFilter::TEarlyExitFilterManager* FilterManager = new PCGExDataFilter::TEarlyExitFilterManager(PointIO);
					FilterManager->Register<UPCGExFilterFactoryBase>(Context, *FilterFactories, PointIO);
					for (int i = 0; i < PointIO->GetNum(); i++) { FilterManager->Test(i); }

					PointFilterCache.Append(FilterManager->Results);
					PCGEX_DELETE(FilterManager)
				}
			}

#pragma endregion

			if (PrimaryOperation) { PrimaryOperation = PrimaryOperation->CopyOperation<UPCGExOperation>(); }

			return true;
		}

		void StartParallelLoopForPoints(const PCGExData::ESource Source = PCGExData::ESource::Out, const int32 PerLoopIterations = -1)
		{
			TArray<FPCGPoint>& Points = PointIO->GetMutableData(Source)->GetMutablePoints();
			const int32 NumPoints = Points.Num();

			if (IsTrivial())
			{
				for (int i = 0; i < NumPoints; i++) { ProcessSinglePoint(i, Points[i]); }
				return;
			}

			int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetPointsBatchIteration(PerLoopIterations);
			int32 CurrentCount = 0;
			while (CurrentCount < NumPoints)
			{
				AsyncManagerPtr->Start<FAsyncProcessPointRange<FPointsProcessor>>(
					CurrentCount, nullptr, this, FMath::Min(NumPoints - CurrentCount, PLI));
				CurrentCount += PLI;
			}
		}

		void StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations = -1)
		{
			if (IsTrivial())
			{
				for (int i = 0; i < NumIterations; i++) { ProcessSingleRangeIteration(i); }
				return;
			}

			int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetPointsBatchIteration(PerLoopIterations);
			int32 CurrentCount = 0;
			while (CurrentCount < NumIterations)
			{
				AsyncManagerPtr->Start<FAsyncProcessRange<FPointsProcessor>>(
					CurrentCount, nullptr, this, FMath::Min(NumIterations - CurrentCount, PLI));
				CurrentCount += PLI;
			}
		}

		virtual void ProcessPoints(const PCGExData::ESource Source, const int32 StartIndex, const int32 Count)
		{
			TArray<FPCGPoint>& Points = PointIO->GetMutableData(Source)->GetMutablePoints();
			for (int i = 0; i < Count; i++)
			{
				const int32 PtIndex = StartIndex + i;
				ProcessSinglePoint(PtIndex, Points[PtIndex]);
			}
		}

		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point)
		{
		}

		virtual void ProcessRange(const int32 StartIndex, const int32 Iterations)
		{
			for (int i = 0; i < Iterations; i++) { ProcessSingleRangeIteration(StartIndex + i); }
		}

		virtual void ProcessSingleRangeIteration(const int32 Iteration)
		{
		}

		virtual void CompleteWork()
		{
		}

		virtual void Write()
		{
		}
	};

	class FPointsProcessorBatchBase
	{
	protected:
		FPCGExAsyncManager* AsyncManagerPtr = nullptr;
		TArray<UPCGExFilterFactoryBase*>* FilterFactories = nullptr;

	public:
		bool bRequiresWriteStep = false;

		mutable FRWLock BatchLock;

		PCGExMT::AsyncState CurrentState = PCGExMT::State_Setup;

		FPCGContext* Context = nullptr;

		TArray<PCGExData::FPointIO*> PointsCollection;

		UPCGExOperation* PrimaryOperation = nullptr;

		FPointsProcessorBatchBase(FPCGContext* InContext, const TArray<PCGExData::FPointIO*>& InPointsCollection):
			Context(InContext), PointsCollection(InPointsCollection)
		{
		}

		virtual ~FPointsProcessorBatchBase()
		{
			Context = nullptr;
			PointsCollection.Empty();
		}

		template <typename T>
		T* GetContext() { return static_cast<T*>(Context); }

		virtual bool PrepareProcessing()
		{
			return true;
		}

		virtual void Process(FPCGExAsyncManager* AsyncManager)
		{
		}

		virtual void CompleteWork()
		{
		}

		virtual void Write()
		{
		}

		void StartParallelLoopForRange(const int32 NumIterations, const int32 PerLoopIterations = -1)
		{
			int32 PLI = GetDefault<UPCGExGlobalSettings>()->GetPointsBatchIteration(PerLoopIterations);
			int32 CurrentCount = 0;
			while (CurrentCount < NumIterations)
			{
				AsyncManagerPtr->Start<FAsyncProcessRange<FPointsProcessorBatchBase>>(
					CurrentCount, nullptr, this, FMath::Min(NumIterations - CurrentCount, PLI));
				CurrentCount += PLI;
			}
		}

		void ProcessRange(const int32 StartIndex, const int32 Iterations)
		{
			for (int i = 0; i < Iterations; i++) { ProcessSingleRangeIteration(StartIndex + i); }
		}

		virtual void ProcessSingleRangeIteration(const int32 Iteration)
		{
		}
	};

	template <typename T>
	class TBatch : public FPointsProcessorBatchBase
	{
	protected:
		bool bInlineProcessing = false;
		bool bInlineCompletion = false;

	public:
		TArray<T*> Processors;
		TArray<T*> ClosedBatchProcessors;

		PCGExMT::AsyncState CurrentState = PCGExMT::State_Setup;

		TBatch(FPCGContext* InContext, const TArray<PCGExData::FPointIO*>& InPointsCollection):
			FPointsProcessorBatchBase(InContext, InPointsCollection)
		{
		}

		virtual ~TBatch() override
		{
			ClosedBatchProcessors.Empty();
			PCGEX_DELETE_TARRAY(Processors)
		}

		void SetPointsFilterData(TArray<UPCGExFilterFactoryBase*>* InFilterFactories)
		{
			FilterFactories = InFilterFactories;
		}

		virtual bool PrepareProcessing() override
		{
			return FPointsProcessorBatchBase::PrepareProcessing();
		}

		virtual void Process(FPCGExAsyncManager* AsyncManager) override
		{
			if (PointsCollection.IsEmpty()) { return; }

			CurrentState = PCGExMT::State_Processing;

			AsyncManagerPtr = AsyncManager;

			for (PCGExData::FPointIO* IO : PointsCollection)
			{
				IO->CreateInKeys();

				T* NewProcessor = new T(IO);
				NewProcessor->Context = Context;

				if (!PrepareSingle(NewProcessor))
				{
					PCGEX_DELETE(NewProcessor)
					continue;
				}

				if (FilterFactories) { NewProcessor->SetPointsFilterData(FilterFactories); }
				if (PrimaryOperation) { NewProcessor->PrimaryOperation = PrimaryOperation; }

				NewProcessor->BatchIndex = Processors.Add(NewProcessor);

				if (IO->GetNum() < GetDefault<UPCGExGlobalSettings>()->SmallPointsSize)
				{
					NewProcessor->bIsSmallPoints = true;
					ClosedBatchProcessors.Add(NewProcessor);
				}

				if (bInlineProcessing) { NewProcessor->Process(AsyncManagerPtr); }
				else if (!NewProcessor->IsTrivial()) { AsyncManager->Start<FAsyncProcess<T>>(IO->IOIndex, IO, NewProcessor); }
			}

			StartClosedBatchProcessing();
		}

		virtual bool PrepareSingle(T* PointsProcessor)
		{
			return true;
		};

		virtual void CompleteWork() override
		{
			CurrentState = PCGExMT::State_Completing;
			if (bInlineCompletion)
			{
				for (T* Processor : Processors) { Processor->CompleteWork(); }
			}
			else
			{
				for (T* Processor : Processors)
				{
					if (Processor->IsTrivial()) { continue; }
					AsyncManagerPtr->Start<FAsyncCompleteWork<T>>(-1, nullptr, Processor);
				}

				StartClosedBatchProcessing();
			}
		}

		virtual void Write() override
		{
			CurrentState = PCGExMT::State_Writing;
			if (bInlineCompletion)
			{
				for (T* Processor : Processors) { Processor->Write(); }
			}
			else
			{
				for (T* Processor : Processors)
				{
					if (Processor->IsTrivial()) { continue; }
					AsyncManagerPtr->Start<FAsyncWrite<T>>(-1, nullptr, Processor);
				}

				StartClosedBatchProcessing();
			}
		}

		void ProcessClosedBatchRange(const int32 StartIndex, const int32 Iterations)
		{
			if (CurrentState == PCGExMT::State_Processing)
			{
				for (int i = 0; i < Iterations; i++) { ClosedBatchProcessors[StartIndex + i]->Process(AsyncManagerPtr); }
			}
			else if (CurrentState == PCGExMT::State_Completing)
			{
				for (int i = 0; i < Iterations; i++) { ClosedBatchProcessors[StartIndex + i]->CompleteWork(); }
			}
			else if (CurrentState == PCGExMT::State_Writing)
			{
				for (int i = 0; i < Iterations; i++) { ClosedBatchProcessors[StartIndex + i]->Write(); }
			}
		}

	protected:
		void StartClosedBatchProcessing()
		{
			const int32 NumTrivial = ClosedBatchProcessors.Num();
			if (NumTrivial > 0)
			{
				int32 CurrentCount = 0;
				while (CurrentCount < ClosedBatchProcessors.Num())
				{
					const int32 PerIterationsNum = GetDefault<UPCGExGlobalSettings>()->PointsDefaultBatchIterations;
					AsyncManagerPtr->Start<FAsyncClosedBatchProcessRange<TBatch<T>>>(
						CurrentCount, nullptr, this, FMath::Min(NumTrivial - CurrentCount, PerIterationsNum));
					CurrentCount += PerIterationsNum;
				}
			}
		}
	};

	static void ScheduleBatch(FPCGExAsyncManager* Manager, FPointsProcessorBatchBase* Batch)
	{
		Manager->Start<FStartPointsBatchProcessing<FPointsProcessorBatchBase>>(-1, nullptr, Batch);
	}

	static void CompleteBatch(FPCGExAsyncManager* Manager, FPointsProcessorBatchBase* Batch)
	{
		Manager->Start<FAsyncCompleteWork<FPointsProcessorBatchBase>>(-1, nullptr, Batch);
	}

	static void WriteBatch(FPCGExAsyncManager* Manager, FPointsProcessorBatchBase* Batch)
	{
		Manager->Start<FAsyncWrite<FPointsProcessorBatchBase>>(-1, nullptr, Batch);
	}
}


#undef PCGEX_POINTS_MT_TASK
#undef PCGEX_POINTS_MT_TASK_RANGE
