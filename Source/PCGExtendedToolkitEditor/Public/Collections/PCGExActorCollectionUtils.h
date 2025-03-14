﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Collections/PCGExActorCollection.h"
#include "Engine/World.h"

class UPackage;

namespace PCGExActorCollectionUtils
{
	static void CreateCollectionFrom(const TArray<FAssetData>& SelectedAssets);
	static void UpdateCollectionsFrom(
		const TArray<TObjectPtr<UPCGExActorCollection>>& SelectedCollections,
		const TArray<FAssetData>& SelectedAssets,
		bool bIsNewCollection = false);
};
