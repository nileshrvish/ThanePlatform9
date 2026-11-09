#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "P9NPCAnimInstance.generated.h"

class UAnimSequence;

/** Worker-side copy of the clip state; evaluates one clip or a crossfade between two. */
struct FP9NPCAnimProxy : public FAnimInstanceProxy
{
	FP9NPCAnimProxy() = default;
	explicit FP9NPCAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	UAnimSequence* Cur = nullptr;
	UAnimSequence* Prev = nullptr;
	float CurTime = 0.0f;
	float PrevTime = 0.0f;
	float Weight = 1.0f;    // weight of Cur (eased)
};

/**
 * Stage 5C: crossfading clip player for the station NPCs (friends, chaiwala, silhouette). Replaces the single-node
 * PlayAnimation switch, which popped from one pose to the next. No Anim Blueprint asset needed: the owner calls
 * PlayClip() with a blend time; the previous clip keeps playing underneath while it fades out.
 */
UCLASS(Transient, NotBlueprintable)
class THANEPLATFORM9_API UP9NPCAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Starts Seq (no-op if it is already the current clip). BlendTime <= 0 switches at once. */
	void PlayClip(UAnimSequence* Seq, bool bLoop, float BlendTime, float StartTime = 0.0f);
	void SetRate(float InRate) { Rate = FMath::Max(0.0f, InRate); }
	void SetTime(float InTime);
	/**
	 * Stage 9A: the clip being faded out keeps playing at StartRate and slows to a stop as the blend completes (instead of
	 * running on at the new clip's rate). Used when a walker stops: the last stride settles instead of scurrying in place.
	 * Applies to the current blend only (reset by the next PlayClip).
	 */
	void SetOutgoingRateDecay(float StartRate) { bPrevDecay = true; PrevRate0 = FMath::Max(0.0f, StartRate); }
	float GetTime() const { return CurTime; }
	UAnimSequence* GetClip() const { return Cur; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FP9NPCAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	friend struct FP9NPCAnimProxy;
	static float Advance(const UAnimSequence* Seq, float Time, float Delta, bool bLoop);

	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Cur;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> Prev;
	float CurTime = 0.0f;
	float PrevTime = 0.0f;
	float Alpha = 1.0f;           // 0 -> 1 over BlendDuration
	float BlendDuration = 0.25f;
	float Rate = 1.0f;
	bool bCurLoop = true;
	bool bPrevLoop = true;
	bool bPrevDecay = false;      // Stage 9A
	float PrevRate0 = 1.0f;
};
