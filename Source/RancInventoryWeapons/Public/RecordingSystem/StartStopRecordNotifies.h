#pragma once

#include "WeaponActor.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "RecordingSystem/WeaponAttackRecorderComponent.h"
#include "StartStopRecordNotifies.generated.h"

UCLASS()
class RANCINVENTORYWEAPONS_API UStartAttackTraceNotify : public UAnimNotify
{
	GENERATED_BODY()

	
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override
	{
		if (UGearManagerComponent* GearManager = MeshComp->GetOwner()->FindComponentByClass<
			UGearManagerComponent>())
		{
			if (auto* ActiveWeapon = GearManager->GetActiveWeapon())
			{
				GearManager->OnAttackTraceStateBeginEnd(true);
#if WITH_EDITOR
				if (GearManager->bRecordAttackTraces)
				if (UWeaponAttackRecorderComponent* WeaponAttackRecorder = ActiveWeapon->FindComponentByClass<
			UWeaponAttackRecorderComponent>())
				{
					WeaponAttackRecorder->OnAnimNotifyBegin(Animation->GetFName());
				}
#endif
			}
		}
	}
};

UCLASS()
class RANCINVENTORYWEAPONS_API UStopAttackTraceNotify : public UAnimNotify
{
	GENERATED_BODY()
	
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override
	{
		if (UGearManagerComponent* GearManager = MeshComp->GetOwner()->FindComponentByClass<
			UGearManagerComponent>())
		{
			if (auto* ActiveWeapon = GearManager->GetActiveWeapon())
			{
				GearManager->OnAttackTraceStateBeginEnd(false);
#if WITH_EDITOR
				if (GearManager->bRecordAttackTraces)
				if (UWeaponAttackRecorderComponent* WeaponAttackRecorder = ActiveWeapon->FindComponentByClass<
			UWeaponAttackRecorderComponent>())
				{
					WeaponAttackRecorder->OnAnimNotifyEnd(Animation->GetFName());
				}
#endif
			}
		}
	}
};

