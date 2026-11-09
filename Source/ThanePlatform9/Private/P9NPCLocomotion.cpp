#include "P9NPCLocomotion.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace P9NPCLoco
{
	static TArray<FBody> GBodies;

	static bool FloorAt(const UWorld* World, const FVector& P, float CurFeetZ, float MaxRise, const FCollisionQueryParams& Params, float& OutZ)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, FVector(P.X, P.Y, CurFeetZ + 60.0f), FVector(P.X, P.Y, CurFeetZ - 250.0f), ECC_Visibility, Params)) return false;
		if (Hit.ImpactPoint.Z - CurFeetZ > MaxRise) return false;           // furniture (bench seat, counter), not floor
		OutZ = Hit.ImpactPoint.Z;
		return true;
	}

	FFloorProbe ProbeFloor(const UWorld* World, const FVector& Feet, const FVector& Dir, float CurFeetZ, const FCollisionQueryParams& Params, float Tread, float MaxRise)
	{
		FFloorProbe R;
		if (!World) return R;
		FVector D = Dir.GetSafeNormal2D();
		if (D.IsNearlyZero()) D = FVector::ForwardVector;
		const float H = Tread * 0.5f - 2.5f;                                   // samples span one tread (midpoint rule)
		auto Z = [&](float Off, float& Out) { return FloorAt(World, Feet + D * Off, CurFeetZ, MaxRise, Params, Out); };
		float Zc = CurFeetZ, Zb = CurFeetZ, Zf = CurFeetZ;
		const bool bC = Z(0.0f, Zc), bB = Z(-H, Zb), bF = Z(H, Zf);
		R.bHit = bC || bB || bF;
		R.bCentreFloor = bC;
		if (!bC) Zc = bB ? Zb : (bF ? Zf : CurFeetZ);
		if (!bB) Zb = Zc;
		if (!bF) Zf = Zc;
		R.CentreZ = Zc; R.RampZ = Zc;
		if (FMath::Abs(Zf - Zb) < 2.0f && FMath::Abs(Zc - Zb) < 2.0f) return R;     // flat
		// a nosing within the window: six samples, then bisect the jump twice (1.25 cm) for its exact position
		const float Off[6] = {-H, -H * 0.6f, -H * 0.2f, H * 0.2f, H * 0.6f, H};
		float S[6];
		for (int32 i = 0; i < 6; ++i)
		{
			if (i == 0) S[i] = Zb; else if (i == 5) S[i] = Zf;
			else if (!Z(Off[i], S[i])) S[i] = Zc;
		}
		int32 Best = 0;
		for (int32 i = 1; i < 5; ++i) if (FMath::Abs(S[i + 1] - S[i]) > FMath::Abs(S[Best + 1] - S[Best])) Best = i;
		const float Lo0 = S[Best], Hi0 = S[Best + 1];
		const float Jump = Hi0 - Lo0;
		if (FMath::Abs(Jump) < 2.0f || FMath::Abs(Jump) > 45.0f) return R;      // kerb, platform edge or noise: no ramp
		float A = Off[Best], B = Off[Best + 1];
		for (int32 k = 0; k < 2; ++k)
		{
			const float M = (A + B) * 0.5f;
			float Zm;
			if (!Z(M, Zm)) break;
			if (FMath::Abs(Zm - Lo0) < FMath::Abs(Zm - Hi0)) A = M; else B = M;
		}
		const float E = (A + B) * 0.5f;                                          // nosing, signed along D
		// the ramp passes through the tread tops at the tread centres, i.e. half a riser at the nosing
		R.RampZ = (Lo0 + Hi0) * 0.5f - Jump * E / Tread;
		R.bOnStairs = true;
		R.StairDir = Jump > 0.0f ? 1 : -1;
		return R;
	}

	float StairPhase(float RampZ, float Rise, int32 StairDir)
	{
		const float V = (StairDir >= 0 ? RampZ : -RampZ) / FMath::Max(1.0f, 2.0f * Rise);
		return V - FMath::FloorToFloat(V);
	}

	void RegisterBody(AActor* Actor, float Radius, float HalfHeight, float FeetOffset, float HalfDepth, float HalfWidth)
	{
		if (!Actor) return;
		UnregisterBody(Actor);
		FBody B; B.Actor = Actor; B.Radius = Radius; B.HalfHeight = HalfHeight; B.FeetOffset = FeetOffset;
		B.HalfDepth = HalfDepth > 0.0f ? HalfDepth : Radius * 0.6f;
		B.HalfWidth = HalfWidth > 0.0f ? HalfWidth : Radius * 0.95f;
		GBodies.Add(B);
	}

	bool BoxOverlap2D(const FVector& A, float YawA, float DA, float WA, const FVector& B, float YawB, float DB, float WB, float Gap, FVector& OutPush)
	{
		const float Ra = FMath::DegreesToRadians(YawA), Rb = FMath::DegreesToRadians(YawB);
		const FVector2D Ax(FMath::Cos(Ra), FMath::Sin(Ra)), Ay(-FMath::Sin(Ra), FMath::Cos(Ra));
		const FVector2D Bx(FMath::Cos(Rb), FMath::Sin(Rb)), By(-FMath::Sin(Rb), FMath::Cos(Rb));
		const FVector2D D(B.X - A.X, B.Y - A.Y);
		const FVector2D Axes[4] = {Ax, Ay, Bx, By};
		float Best = TNumericLimits<float>::Max();
		FVector2D BestAxis = Ax;
		for (const FVector2D& N : Axes)
		{
			const float Pa = DA * FMath::Abs(FVector2D::DotProduct(Ax, N)) + WA * FMath::Abs(FVector2D::DotProduct(Ay, N));
			const float Pb = DB * FMath::Abs(FVector2D::DotProduct(Bx, N)) + WB * FMath::Abs(FVector2D::DotProduct(By, N));
			const float Dist = FVector2D::DotProduct(D, N);
			const float Over = Pa + Pb + Gap - FMath::Abs(Dist);
			if (Over <= 0.0f) return false;                                  // a separating axis: no overlap
			if (Over < Best) { Best = Over; BestAxis = Dist >= 0.0f ? -N : N; }
		}
		OutPush = FVector(BestAxis.X * Best, BestAxis.Y * Best, 0.0f);
		return true;
	}

	int32 CountBoxOverlaps(const UWorld* World, float& OutMaxDepth, FString* OutPairs)
	{
		OutMaxDepth = 0.0f;
		TArray<FBody> All;
		for (const FBody& B : GBodies)
		{
			const AActor* Act = B.Actor.Get();
			if (Act && Act->GetWorld() == World && !Act->IsHidden()) All.Add(B);
		}
		if (const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
		{
			if (APawn* P = PC->GetPawn())
			{
				FBody PB; PB.Actor = P; PB.Radius = 34.0f; PB.HalfDepth = 30.0f; PB.HalfWidth = 30.0f; PB.FeetOffset = 90.0f;
				All.Add(PB);
			}
		}
		int32 Count = 0;
		for (int32 i = 0; i < All.Num(); ++i)
		{
			for (int32 j = i + 1; j < All.Num(); ++j)
			{
				const AActor* A = All[i].Actor.Get();
				const AActor* B = All[j].Actor.Get();
				const FVector FA = A->GetActorLocation() - FVector(0, 0, All[i].FeetOffset), FB = B->GetActorLocation() - FVector(0, 0, All[j].FeetOffset);
				if (FMath::Abs(FA.Z - FB.Z) > 120.0f) continue;
				FVector Push;
				if (BoxOverlap2D(FA, A->GetActorRotation().Yaw, All[i].HalfDepth, All[i].HalfWidth, FB, B->GetActorRotation().Yaw, All[j].HalfDepth, All[j].HalfWidth, 0.0f, Push))
				{
					++Count;
					OutMaxDepth = FMath::Max(OutMaxDepth, static_cast<float>(Push.Size()));
					if (OutPairs) *OutPairs += FString::Printf(TEXT(" %s~%s(%.1fcm)"), *A->GetName(), *B->GetName(), Push.Size());
				}
			}
		}
		return Count;
	}

	void UnregisterBody(AActor* Actor)
	{
		GBodies.RemoveAll([Actor](const FBody& B) { return !B.Actor.IsValid() || B.Actor.Get() == Actor; });
	}

	void GatherBodies(const UWorld* World, const AActor* Self, TArray<FBody>& Out)
	{
		Out.Reset();
		for (const FBody& B : GBodies)
		{
			const AActor* A = B.Actor.Get();
			if (!A || A == Self || A->GetWorld() != World || A->IsHidden()) continue;
			Out.Add(B);
		}
	}

	bool MoveHitsWorld(const UWorld* World, const FVector& FromFeet, const FVector& ToFeet, float Radius, const FCollisionQueryParams& Params)
	{
		if (!World || FVector::DistSquared2D(FromFeet, ToFeet) < 1.0f) return false;
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		const FVector Up(0.0f, 0.0f, 100.0f);                                   // chest height: walls, parapets, closures (not steps)
		return World->SweepTestByObjectType(FromFeet + Up, FVector(ToFeet.X, ToFeet.Y, FromFeet.Z) + Up, FQuat::Identity, Objects,
			FCollisionShape::MakeSphere(Radius * 0.8f), Params);
	}
}
