#include "P9NPCAnimInstance.h"

#include "AnimationRuntime.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"

float UP9NPCAnimInstance::Advance(const UAnimSequence* Seq, float Time, float Delta, bool bLoop)
{
	if (!Seq) return 0.0f;
	const float Len = FMath::Max(0.01f, Seq->GetPlayLength());
	Time += Delta;
	return bLoop ? FMath::Fmod(FMath::Max(0.0f, Time), Len) : FMath::Clamp(Time, 0.0f, Len);
}

void UP9NPCAnimInstance::PlayClip(UAnimSequence* Seq, bool bLoop, float BlendTime, float StartTime)
{
	if (!Seq || (Seq == Cur && bLoop == bCurLoop)) return;
	if (Cur && BlendTime > 0.0f)
	{
		// mid-blend: the more visible of the two keeps fading out (avoids a pop when clips change quickly)
		if (Prev && Alpha < 0.5f) { /* keep Prev */ }
		else { Prev = Cur; PrevTime = CurTime; bPrevLoop = bCurLoop; }
		Alpha = 0.0f;
		BlendDuration = BlendTime;
	}
	else
	{
		Prev = nullptr;
		Alpha = 1.0f;
	}
	Cur = Seq;
	bCurLoop = bLoop;
	CurTime = Advance(Seq, StartTime, 0.0f, bLoop);
}

void UP9NPCAnimInstance::SetTime(float InTime)
{
	CurTime = Advance(Cur, InTime, 0.0f, bCurLoop);
}

void UP9NPCAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const float D = DeltaSeconds * Rate;
	CurTime = Advance(Cur, CurTime, D, bCurLoop);
	if (Prev && Alpha < 1.0f)
	{
		PrevTime = Advance(Prev, PrevTime, D, bPrevLoop);
		Alpha = FMath::Min(1.0f, Alpha + DeltaSeconds / FMath::Max(0.01f, BlendDuration));
		if (Alpha >= 1.0f) Prev = nullptr;
	}
}

void FP9NPCAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (const UP9NPCAnimInstance* I = Cast<UP9NPCAnimInstance>(InAnimInstance))
	{
		Cur = I->Cur;
		Prev = (I->Prev && I->Alpha < 1.0f) ? I->Prev.Get() : nullptr;
		CurTime = I->CurTime;
		PrevTime = I->PrevTime;
		Weight = FMath::SmoothStep(0.0f, 1.0f, I->Alpha);
	}
}

bool FP9NPCAnimProxy::Evaluate(FPoseContext& Output)
{
	if (!Cur)
	{
		Output.ResetToRefPose();
		return true;
	}
	FAnimationPoseData OutData(Output);
	const FAnimExtractContext CurCtx(static_cast<double>(CurTime), false);
	if (!Prev || Weight >= 0.999f)
	{
		Cur->GetAnimationPose(OutData, CurCtx);
		return true;
	}
	FPoseContext PrevPose(Output);
	FPoseContext CurPose(Output);
	FAnimationPoseData PrevData(PrevPose);
	FAnimationPoseData CurData(CurPose);
	Prev->GetAnimationPose(PrevData, FAnimExtractContext(static_cast<double>(PrevTime), false));
	Cur->GetAnimationPose(CurData, CurCtx);
	FAnimationRuntime::BlendTwoPosesTogether(PrevData, CurData, 1.0f - Weight, OutData);
	return true;
}
