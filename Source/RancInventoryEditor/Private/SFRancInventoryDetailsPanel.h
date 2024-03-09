// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>
#include <PropertyCustomizationHelpers.h>

class SFRancInventoryItemDetailsPanel final : public IPropertyTypeCustomization
{
public:
    static TSharedRef<IPropertyTypeCustomization> MakeItemInstance()
    {
        return MakeShared<SFRancInventoryItemDetailsPanel>();
    }

protected:
    virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

    virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

    FString GetObjPath() const;
    void OnObjChanged(const FAssetData& AssetData) const;

private:
    TSharedPtr<IPropertyHandle> PropertyHandlePtr;
};


class SFRancInventoryRecipeDetailsPanel final : public IPropertyTypeCustomization
{
public:

    static TSharedRef<IPropertyTypeCustomization> MakeRecipeInstance()
    {
        return MakeShared<SFRancInventoryRecipeDetailsPanel>();
    }

protected:
    virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

    virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

    FString GetObjPath() const;
    void OnObjChanged(const FAssetData& AssetData) const;

private:
    TSharedPtr<IPropertyHandle> PropertyHandlePtr;
};
