#include "TestClass.generated.h"

#pragma once



UCLASS(Blueprintable, ClassGroup = (Custom), Category = "Ranc Inventory | Classes", EditInlineNew, meta = (BlueprintSpawnableComponent))
class UTestClass : public UActorComponent
{
	GENERATED_BODY()
public:

	void A();
};