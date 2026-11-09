#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;
struct FCollisionQueryParams;

/**
 * Stage 7B: shared locomotion helpers for the station NPCs (AP9FriendNPC) and Kanchana (AP9SilhouetteStalker).
 *
 * Stairs. The station's stairs are stepped collision (rise 16.667 cm, tread 30 cm, step tops at whole rises above the
 * platforms). A single floor trace makes a walker's root jump a riser at every nosing. ProbeFloor instead returns the
 * smooth ramp through the tread tops at the tread centres (the line the fitted stair clips are authored against) by locating
 * the nosing under the body exactly, plus the stair direction relative to the way the body moves. StairPhase turns the
 * root height into the clip phase, so a fitted stair clip lands a foot on every tread whatever the walking speed.
 *
 * Bodies. A tiny registry of NPC bodies (capsule radius / half height, world position) used for NPC-to-NPC separation.
 * No custom collision channel is needed (project settings stay untouched); the cost is a loop over at most a handful of
 * actors per moving NPC per tick.
 */
namespace P9NPCLoco
{
	struct FFloorProbe
	{
		bool bHit = false;          // any floor found
		bool bCentreFloor = false;  // the trace straight under the body found floor (not a wall top / furniture, not a drop)
		bool bOnStairs = false;     // a nosing lies within half a tread of the body (along Dir)
		int32 StairDir = 0;         // +1 the floor rises along Dir (going up), -1 going down, 0 flat
		float RampZ = 0.0f;         // floor height for the root: the stair ramp on stairs, the floor elsewhere
		float CentreZ = 0.0f;       // plain floor height straight under the body
	};

	/**
	 * Floor under Feet (x/y used) for a body moving along Dir (2D unit). CurFeetZ: current feet height; floor hits more than
	 * MaxRise above it (bench seats, counters) are ignored. 3 traces on flat floor, 8 when a nosing is under the body.
	 */
	THANEPLATFORM9_API FFloorProbe ProbeFloor(const UWorld* World, const FVector& Feet, const FVector& Dir, float CurFeetZ,
		const FCollisionQueryParams& Params, float Tread = 30.0f, float MaxRise = 35.0f);

	/** Clip phase 0..1 for a fitted stair clip: frac(z / 2 rise) going up, frac(-z / 2 rise) going down. */
	THANEPLATFORM9_API float StairPhase(float RampZ, float Rise, int32 StairDir);

	struct FBody
	{
		TWeakObjectPtr<AActor> Actor;
		float Radius = 28.0f;
		float HalfHeight = 88.0f;
		float FeetOffset = 90.0f;   // actor Z - feet Z (friends 90, Kanchana 0)
		/** Stage 8A: the body as a box aligned with the actor (half depth along its facing, half width across the shoulders). */
		float HalfDepth = 17.0f;
		float HalfWidth = 26.0f;
	};
	/** HalfDepth / HalfWidth 0: derived from the radius (0.6 r deep, 0.95 r wide - a person is wider than deep). */
	THANEPLATFORM9_API void RegisterBody(AActor* Actor, float Radius, float HalfHeight, float FeetOffset, float HalfDepth = 0.0f, float HalfWidth = 0.0f);
	/**
	 * Stage 8A: 2D separating-axis test of two yawed boxes (centres A/B, yaw degrees, half depth / half width), each grown by
	 * Gap / 2. True when they overlap; OutPush is the shortest move of A that separates them (z = 0).
	 */
	THANEPLATFORM9_API bool BoxOverlap2D(const FVector& A, float YawA, float DA, float WA, const FVector& B, float YawB, float DB, float WB, float Gap, FVector& OutPush);
	/** Test helper: pairs of visible registered bodies (and the player, 34 cm square) whose boxes overlap now, and the deepest overlap. */
	THANEPLATFORM9_API int32 CountBoxOverlaps(const UWorld* World, float& OutMaxDepth, FString* OutPairs = nullptr);
	THANEPLATFORM9_API void UnregisterBody(AActor* Actor);
	/** Visible bodies of World other than Self (hidden NPCs - locked friends, hidden Kanchana - are skipped). */
	THANEPLATFORM9_API void GatherBodies(const UWorld* World, const AActor* Self, TArray<FBody>& Out);
	/** True if a straight horizontal move of a body (Radius) from From to To at chest height hits world geometry. */
	THANEPLATFORM9_API bool MoveHitsWorld(const UWorld* World, const FVector& FromFeet, const FVector& ToFeet, float Radius, const FCollisionQueryParams& Params);
}
