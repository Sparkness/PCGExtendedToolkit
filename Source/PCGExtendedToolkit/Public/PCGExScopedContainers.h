﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGExMT.h"

namespace PCGExMT
{
	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TScopedArray final : public TSharedFromThis<TScopedArray<T>>
	{
	public:
		TArray<TSharedPtr<TArray<T>>> Values;

		explicit TScopedArray(const TArray<FScope>& InScopes, const T InDefaultValue)
		{
			Values.Reserve(InScopes.Num());
			for (const FScope& Scope : InScopes) { Values[Values.Add(MakeShared<TArray<T>>())]->Init(InDefaultValue, Scope.Count); }
		};

		explicit TScopedArray(const TArray<FScope>& InScopes)
		{
			Values.Reserve(InScopes.Num());
			for (int i = 0; i < InScopes.Num(); i++) { Values.Add(MakeShared<TArray<T>>()); }
		};

		 ~TScopedArray() = default;

		FORCEINLINE TSharedPtr<TArray<T>> Get(const FScope& InScope) { return Values[InScope.LoopIndex]; }
		FORCEINLINE TArray<T>& Get_Ref(const FScope& InScope) { return *Values[InScope.LoopIndex].Get(); }

		using FForEachFunc = std::function<void (TArray<T>&)>;
		FORCEINLINE void ForEach(FForEachFunc&& Func) { for (int i = 0; i < Values.Num(); i++) { Func(*Values[i].Get()); } }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TScopedSet final : public TSharedFromThis<TScopedSet<T>>
	{
	public:
		TArray<TSharedPtr<TSet<T>>> Sets;

		explicit TScopedSet(const TArray<FScope>& InScopes, const T InReserve)
		{
			Sets.Reserve(InScopes.Num());
			for (int i = 0; i < InScopes.Num(); i++) { Sets.Add_GetRef(MakeShared<TSet<T>>())->Reserve(InReserve); }
		};

		~TScopedSet() = default;

		FORCEINLINE TSharedPtr<TSet<T>> Get(const FScope& InScope) { return Sets[InScope.LoopIndex]; }
		FORCEINLINE TSet<T>& Get_Ref(const FScope& InScope) { return *Sets[InScope.LoopIndex].Get(); }

		using FForEachFunc = std::function<void (TSet<T>&)>;
		FORCEINLINE void ForEach(FForEachFunc&& Func) { for (int i = 0; i < Sets.Num(); i++) { Func(*Sets[i].Get()); } }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TScopedValue final : public TSharedFromThis<TScopedValue<T>>
	{
	public:
		TArray<T> Values;

		using FFlattenFunc = std::function<T(const T&, const T&)>;

		explicit TScopedValue(const TArray<FScope>& InScopes, const T InDefaultValue)
		{
			Values.Init(InDefaultValue, InScopes.Num());
		};

		~TScopedValue() = default;

		FORCEINLINE T Get(const FScope& InScope) { return Values[InScope.LoopIndex]; }
		FORCEINLINE T& GetMutable(const FScope& InScope) { return Values[InScope.LoopIndex]; }
		FORCEINLINE T Set(const FScope& InScope, const T& InValue) { return Values[InScope.LoopIndex] = InValue; }

		FORCEINLINE T Flatten(FFlattenFunc&& Func)
		{
			T Result = Values[0];
			if (Values.Num() > 1) { for (int i = 1; i < Values.Num(); i++) { Result = Func(Values[i], Result); } }
			return Result;
		}
	};
}
