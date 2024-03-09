// Copyright Rancorous Games, 2024

#include "SFRancInventoryDetailsPanel.h"
#include <..\..\RancInventory\Public\Management\RISInventoryData.h>
#include <Engine/AssetManager.h>

void SFRancInventoryItemDetailsPanel::CustomizeHeader(const TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    PropertyHandlePtr = PropertyHandle;

    HeaderRow
        .NameContent()
        [
            PropertyHandle->CreatePropertyNameWidget()
        ]
        .ValueContent()
        [
            SNew(SObjectPropertyEntryBox)
                .AllowedClass(URISItemData::StaticClass())
                .PropertyHandle(PropertyHandle)
                .DisplayThumbnail(true)
                .ThumbnailPool(CustomizationUtils.GetThumbnailPool())
                .ObjectPath(this, &SFRancInventoryItemDetailsPanel::GetObjPath)
                .OnObjectChanged(this, &SFRancInventoryItemDetailsPanel::OnObjChanged)
                .OnShouldFilterAsset_Lambda(
                    [](const FAssetData& AssetData) -> bool
                    {
                        return AssetData.GetPrimaryAssetId().PrimaryAssetType != FPrimaryAssetType(RancInventoryItemDataType);
                    }
                )
        ];
}

void SFRancInventoryItemDetailsPanel::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void SFRancInventoryItemDetailsPanel::OnObjChanged(const FAssetData& AssetData) const
{
    // (PrimaryAssetType="VALUE",PrimaryAssetName="VALUE")
    const FString InValue(FString::Printf(TEXT("(PrimaryAssetType=\"%s\",PrimaryAssetName=\"%s\")"), *AssetData.GetPrimaryAssetId().PrimaryAssetType.ToString(), *AssetData.GetPrimaryAssetId().PrimaryAssetName.ToString()));

    ensure(PropertyHandlePtr->SetValueFromFormattedString(InValue) == FPropertyAccess::Result::Success);
}

FString SFRancInventoryItemDetailsPanel::GetObjPath() const
{
    if (const UAssetManager* const AssetManager = UAssetManager::GetIfValid();
        AssetManager && PropertyHandlePtr.IsValid())
    {
        FString AssetTypeValueStr;
        PropertyHandlePtr->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPrimaryAssetId, PrimaryAssetType))->GetValueAsDisplayString(AssetTypeValueStr);

        FString AssetIdValueStr;
        PropertyHandlePtr->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPrimaryAssetId, PrimaryAssetName))->GetValueAsDisplayString(AssetIdValueStr);

        const FPrimaryAssetId AssetId(*AssetTypeValueStr, *AssetIdValueStr);
        const FString Output = AssetId.IsValid() ? AssetManager->GetPrimaryAssetPath(AssetId).ToString() : FString();

        if (AssetId.IsValid() && Output.IsEmpty())
        {
            FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Asset Manager could not retrieve asset information: Check if you've added the path to the Asset Manager settings.")));

            PropertyHandlePtr->ResetToDefault();
        }

        return Output;
    }

    return FString();
}

void SFRancInventoryRecipeDetailsPanel::CustomizeHeader(const TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    PropertyHandlePtr = PropertyHandle;

    HeaderRow
        .NameContent()
        [
            PropertyHandle->CreatePropertyNameWidget()
        ]
        .ValueContent()
        [
            SNew(SObjectPropertyEntryBox)
                .AllowedClass(URISObjectRecipeData::StaticClass())
                .PropertyHandle(PropertyHandle)
                .DisplayThumbnail(true)
                .ThumbnailPool(CustomizationUtils.GetThumbnailPool())
                .ObjectPath(this, &SFRancInventoryRecipeDetailsPanel::GetObjPath)
                .OnObjectChanged(this, &SFRancInventoryRecipeDetailsPanel::OnObjChanged)
                .OnShouldFilterAsset_Lambda(
                    [](const FAssetData& AssetData) -> bool
                    {
                        return AssetData.GetPrimaryAssetId().PrimaryAssetType != FPrimaryAssetType(RancInventoryRecipeDataType);
                    }
                )
        ];
}

void SFRancInventoryRecipeDetailsPanel::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void SFRancInventoryRecipeDetailsPanel::OnObjChanged(const FAssetData& AssetData) const
{
    // (PrimaryAssetType="VALUE",PrimaryAssetName="VALUE")
    const FString InValue(FString::Printf(TEXT("(PrimaryAssetType=\"%s\",PrimaryAssetName=\"%s\")"), *AssetData.GetPrimaryAssetId().PrimaryAssetType.ToString(), *AssetData.GetPrimaryAssetId().PrimaryAssetName.ToString()));

    ensure(PropertyHandlePtr->SetValueFromFormattedString(InValue) == FPropertyAccess::Result::Success);
}

FString SFRancInventoryRecipeDetailsPanel::GetObjPath() const
{
    if (const UAssetManager* const AssetManager = UAssetManager::GetIfValid();
        AssetManager && PropertyHandlePtr.IsValid())
    {
        FString AssetTypeValueStr;
        PropertyHandlePtr->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPrimaryAssetId, PrimaryAssetType))->GetValueAsDisplayString(AssetTypeValueStr);

        FString AssetIdValueStr;
        PropertyHandlePtr->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPrimaryAssetId, PrimaryAssetName))->GetValueAsDisplayString(AssetIdValueStr);

        const FPrimaryAssetId AssetId(*AssetTypeValueStr, *AssetIdValueStr);
        const FString Output = AssetId.IsValid() ? AssetManager->GetPrimaryAssetPath(AssetId).ToString() : FString();

        if (AssetId.IsValid() && Output.IsEmpty())
        {
            FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Asset Manager could not retrieve asset information: Check if you've added the path to the Asset Manager settings.")));

            PropertyHandlePtr->ResetToDefault();
        }

        return Output;
    }

    return FString();
}