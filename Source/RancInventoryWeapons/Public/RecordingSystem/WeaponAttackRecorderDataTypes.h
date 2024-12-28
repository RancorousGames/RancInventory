USTRUCT(BlueprintType)
struct FWeaponAttackTrace
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector StartOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector EndOffset = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FWeaponAttackTimestamp
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FWeaponAttackTrace> TracesAtTime;
};

UCLASS(BlueprintType)
class UWeaponAttackData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FWeaponAttackTimestamp> AttackSequence;
};