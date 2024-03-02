// Copyright Rancorous Games, 2024

#pragma once

#include <CoreMinimal.h>
#include <PropertyCustomizationHelpers.h>

class SFRancInventoryDetailsPanel final : public IPropertyTypeCustomization
{
public:
    static TSharedRef<IPropertyTypeCustomization> MakeInstance()
    {
        return MakeShared<SFRancInventoryDetailsPanel>();
    }

protected:
    virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

    virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

    FString GetObjPath() const;
    void OnObjChanged(const FAssetData& AssetData) const;

private:
    TSharedPtr<IPropertyHandle> PropertyHandlePtr;
};
