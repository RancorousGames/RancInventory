#pragma once

#include <CoreMinimal.h>
#include "ItemDefinitionBase.generated.h"

// Base class for all item definitions (component/fragment like subuobjects for item static data)
UCLASS(Blueprintable, EditInlineNew, Abstract, DefaultToInstanced)
class RANCINVENTORY_API UItemDefinitionBase : public UObject
{
	GENERATED_BODY()
};
