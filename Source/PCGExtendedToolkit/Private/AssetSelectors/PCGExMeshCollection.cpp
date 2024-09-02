// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetSelectors/PCGExMeshCollection.h"

#include "PCGEx.h"
#include "PCGExMacros.h"

bool FPCGExMeshCollectionEntry::Validate(const UPCGExAssetCollection* ParentCollection)
{
	if (bIsSubCollection) { LoadSubCollection(SubCollection); }
	else if (!Descriptor.StaticMesh.ToSoftObjectPath().IsValid() && ParentCollection->bDoNotIgnoreInvalidEntries) { return false; }

	return Super::Validate(ParentCollection);
}

void FPCGExMeshCollectionEntry::UpdateStaging(const UPCGExAssetCollection* OwningCollection, const bool bRecursive)
{
	if (bIsSubCollection)
	{
		if (bRecursive && SubCollection.LoadSynchronous()) { SubCollection.Get()->EDITOR_RebuildStagingData_Recursive(); }
		return;
	}

	Staging.Path = Descriptor.StaticMesh.ToSoftObjectPath();

	const UStaticMesh* M = Descriptor.StaticMesh.LoadSynchronous();
	PCGExAssetCollection::UpdateStagingBounds(Staging, M);

	Super::UpdateStaging(OwningCollection, bRecursive);
}

void FPCGExMeshCollectionEntry::SetAssetPath(FSoftObjectPath InPath)
{
	Descriptor.StaticMesh = TSoftObjectPtr<UStaticMesh>(InPath);
}

void FPCGExMeshCollectionEntry::OnSubCollectionLoaded()
{
	SubCollectionPtr = Cast<UPCGExMeshCollection>(BaseSubCollectionPtr);
}

void UPCGExMeshCollection::RebuildStagingData(const bool bRecursive)
{
	for (FPCGExMeshCollectionEntry& Entry : Entries) { Entry.UpdateStaging(this, bRecursive); }
	Super::RebuildStagingData(bRecursive);
}

#if WITH_EDITOR
bool UPCGExMeshCollection::EDITOR_IsCacheableProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Super::EDITOR_IsCacheableProperty(PropertyChangedEvent)) { return true; }
	return PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UPCGExMeshCollection, Entries);
}

void UPCGExMeshCollection::EDITOR_RefreshDisplayNames()
{
	Super::EDITOR_RefreshDisplayNames();
	for (FPCGExMeshCollectionEntry& Entry : Entries)
	{
		Entry.DisplayName = Entry.bIsSubCollection ? FName(TEXT("[") + Entry.SubCollection.GetAssetName() + TEXT("]")) : FName(Entry.Descriptor.StaticMesh.GetAssetName());
	}
}

UPCGExAssetCollection* UPCGExMeshCollection::GetCollectionFromAttributeSet(const FPCGContext* InContext, const UPCGParamData* InAttributeSet, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging) const
{
	return GetCollectionFromAttributeSetTpl<UPCGExMeshCollection>(InContext, InAttributeSet, Details, bBuildStaging);
}

UPCGExAssetCollection* UPCGExMeshCollection::GetCollectionFromAttributeSet(const FPCGContext* InContext, const FName InputPin, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging) const
{
	return GetCollectionFromAttributeSetTpl<UPCGExMeshCollection>(InContext, InputPin, Details, bBuildStaging);
}

void UPCGExMeshCollection::GetAssetPaths(TSet<FSoftObjectPath>& OutPaths, const PCGExAssetCollection::ELoadingFlags Flags) const
{
	for (const FPCGExMeshCollectionEntry& Entry : Entries)
	{
		if (Entry.bIsSubCollection)
		{
			if (Flags == PCGExAssetCollection::ELoadingFlags::Recursive)
			{
				if (const UPCGExMeshCollection* SubCollection = Entry.SubCollection.LoadSynchronous())
				{
					SubCollection->GetAssetPaths(OutPaths, Flags);
				}
			}
			continue;
		}

		if (!Entry.Descriptor.StaticMesh.Get()) { OutPaths.Add(Entry.Descriptor.StaticMesh.ToSoftObjectPath()); }
	}
}

void UPCGExMeshCollection::BuildCache()
{
	Super::BuildCache(Entries);
}
#endif
