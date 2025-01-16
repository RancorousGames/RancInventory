#pragma once

#include <CoreMinimal.h>
#include "ItemDefinitionBase.generated.h"

// Base class for all item definitions (component/fragment like subuobjects for item static data)
UCLASS(Blueprintable, EditInlineNew)
class RANCINVENTORY_API UItemDefinitionBase : public UObject
{
	GENERATED_BODY()
public:
};

// Base class for all item definitions (component/fragment like subuobjects for item static data)
UCLASS(Blueprintable, DefaultToInstanced)
class RANCINVENTORY_API UItemDefinitionBaseX : public UObject// DOES NOT WORK
{
	GENERATED_BODY()
public:
};


// Base class for all item definitions (component/fragment like subuobjects for item static data)

UCLASS(Blueprintable, ClassGroup = (Custom), Category = "RIS | Classes", EditInlineNew, meta = (BlueprintSpawnableComponent))
class RANCINVENTORY_API UItemDefinitionBaseXX : public UObject // WORKS
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS | Classes | Data")
	int32 blah;
};


UCLASS(DefaultToInstanced, BlueprintType, meta=(config=Engine, MinimalAPI))
class RANCINVENTORY_API UItemDefinitionBaseXXX : public UObject // DOES NOT WORK
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RIS | Classes | Data")
	int32 blah;
};
