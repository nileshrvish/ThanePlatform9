// Sprint 27 — Animation Authoring
#include "MCPServer.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "EditorAssetLibrary.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Factories/AnimMontageFactory.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Factories/BlendSpaceFactoryNew.h"
#include "Factories/BlendSpaceFactory1D.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_LocalRefPose.h"
#include "AnimGraphNode_LinkedInputPose.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimationGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_VariableGet.h"
#include "EditorAnimUtils.h"
#include "Animation/AnimationAsset.h"
#include "AnimPose.h"
#include "AnimationBlueprintLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimationStateMachineGraph.h"
#include "AnimStateNode.h"
#include "AnimStateNodeBase.h"
#include "AnimStateEntryNode.h"
#include "AnimStateTransitionNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_BreakStruct.h"
#include "Kismet/KismetMathLibrary.h"
#include "MCPServerHelpers.h"

// ===== MONTAGE =====

FString FMCPServer::HandleCreateMontage(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath, AssetName, SkeletonPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));
	if (!Params->TryGetStringField(TEXT("skeleton_path"), SkeletonPath))
		return MakeError(TEXT("skeleton_path required (path to USkeleton asset)"));

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
		return MakeError(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
	Factory->TargetSkeleton = Skeleton;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, UAnimMontage::StaticClass(), Factory);
	if (!NewAsset)
		return MakeError(TEXT("Failed to create AnimMontage"));

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("skeleton"), SkeletonPath);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleAddMontageSection(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString MontagePath, SectionName;
	if (!Params->TryGetStringField(TEXT("montage_path"), MontagePath))
		return MakeError(TEXT("montage_path required"));
	if (!Params->TryGetStringField(TEXT("section_name"), SectionName))
		return MakeError(TEXT("section_name required"));

	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	if (!Montage)
		return MakeError(FString::Printf(TEXT("Montage not found: %s"), *MontagePath));

	// Check if section exists
	int32 ExistingIdx = Montage->GetSectionIndex(FName(*SectionName));
	if (ExistingIdx != INDEX_NONE)
		return MakeError(FString::Printf(TEXT("Section already exists: %s (index %d)"), *SectionName, ExistingIdx));

	// Add section
	FCompositeSection NewSection;
	NewSection.SectionName = FName(*SectionName);

	double StartTime = 0;
	Params->TryGetNumberField(TEXT("start_time"), StartTime);
	NewSection.SetTime(static_cast<float>(StartTime));
	Montage->CompositeSections.Add(NewSection);
	Montage->Modify();
	UEditorAssetLibrary::SaveAsset(MontagePath, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("section"), SectionName);
	Data->SetNumberField(TEXT("start_time"), StartTime);
	Data->SetNumberField(TEXT("total_sections"), Montage->CompositeSections.Num());
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadMontage(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString MontagePath;
	if (!Params->TryGetStringField(TEXT("path"), MontagePath))
		return MakeError(TEXT("path required"));

	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	if (!Montage)
		return MakeError(FString::Printf(TEXT("Montage not found: %s"), *MontagePath));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), MontagePath);
	Data->SetNumberField(TEXT("length"), Montage->GetPlayLength());
	Data->SetStringField(TEXT("skeleton"), Montage->GetSkeleton() ? Montage->GetSkeleton()->GetPathName() : TEXT("None"));

	// Sections
	TArray<TSharedPtr<FJsonValue>> Sections;
	for (int32 i = 0; i < Montage->CompositeSections.Num(); ++i)
	{
		TSharedPtr<FJsonObject> S = MakeShared<FJsonObject>();
		S->SetStringField(TEXT("name"), Montage->CompositeSections[i].SectionName.ToString());
		S->SetNumberField(TEXT("time"), Montage->CompositeSections[i].GetTime());
		Sections.Add(MakeShared<FJsonValueObject>(S));
	}
	Data->SetArrayField(TEXT("sections"), Sections);

	// Slot tracks
	TArray<TSharedPtr<FJsonValue>> Slots;
	for (const FSlotAnimationTrack& SlotTrack : Montage->SlotAnimTracks)
	{
		TSharedPtr<FJsonObject> SlotObj = MakeShared<FJsonObject>();
		SlotObj->SetStringField(TEXT("slot_name"), SlotTrack.SlotName.ToString());
		Slots.Add(MakeShared<FJsonValueObject>(SlotObj));
	}
	Data->SetArrayField(TEXT("slots"), Slots);

	// Notifies
	TArray<TSharedPtr<FJsonValue>> Notifies;
	for (const FAnimNotifyEvent& Notify : Montage->Notifies)
	{
		TSharedPtr<FJsonObject> N = MakeShared<FJsonObject>();
		N->SetStringField(TEXT("name"), Notify.NotifyName.ToString());
		N->SetNumberField(TEXT("time"), Notify.GetTriggerTime());
		N->SetStringField(TEXT("class"), Notify.Notify ? Notify.Notify->GetClass()->GetName() : TEXT("None"));
		Notifies.Add(MakeShared<FJsonValueObject>(N));
	}
	Data->SetArrayField(TEXT("notifies"), Notifies);

	return MakeResponse(true, Data);
}

// ===== BLEND SPACE =====

FString FMCPServer::HandleCreateBlendSpace(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath, AssetName, SkeletonPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));
	if (!Params->TryGetStringField(TEXT("skeleton_path"), SkeletonPath))
		return MakeError(TEXT("skeleton_path required"));

	bool bIs1D = false;
	Params->TryGetBoolField(TEXT("is_1d"), bIs1D);

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
		return MakeError(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UFactory* Factory = nullptr;
	UClass* AssetClass = nullptr;
	if (bIs1D)
	{
		UBlendSpaceFactory1D* BSFactory = NewObject<UBlendSpaceFactory1D>();
		BSFactory->TargetSkeleton = Skeleton;
		Factory = BSFactory;
		AssetClass = UBlendSpace1D::StaticClass();
	}
	else
	{
		UBlendSpaceFactoryNew* BSFactory = NewObject<UBlendSpaceFactoryNew>();
		BSFactory->TargetSkeleton = Skeleton;
		Factory = BSFactory;
		AssetClass = UBlendSpace::StaticClass();
	}

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, AssetClass, Factory);
	if (!NewAsset)
		return MakeError(TEXT("Failed to create BlendSpace"));

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Data->SetBoolField(TEXT("is_1d"), bIs1D);
	Data->SetStringField(TEXT("skeleton"), SkeletonPath);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleAddBlendSpaceSample(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString BlendSpacePath, AnimPath;
	if (!Params->TryGetStringField(TEXT("blend_space_path"), BlendSpacePath))
		return MakeError(TEXT("blend_space_path required"));
	if (!Params->TryGetStringField(TEXT("animation_path"), AnimPath))
		return MakeError(TEXT("animation_path required (AnimSequence asset path)"));

	double ValueX = 0, ValueY = 0;
	Params->TryGetNumberField(TEXT("value_x"), ValueX);
	Params->TryGetNumberField(TEXT("value_y"), ValueY);

	UBlendSpace* BlendSpace = LoadObject<UBlendSpace>(nullptr, *BlendSpacePath);
	if (!BlendSpace)
		return MakeError(FString::Printf(TEXT("BlendSpace not found: %s"), *BlendSpacePath));

	UAnimSequence* AnimSeq = LoadObject<UAnimSequence>(nullptr, *AnimPath);
	if (!AnimSeq)
		return MakeError(FString::Printf(TEXT("AnimSequence not found: %s"), *AnimPath));

	// Add sample via editing API
	BlendSpace->Modify();
	BlendSpace->AddSample(AnimSeq, FVector(ValueX, ValueY, 0.0));

	UEditorAssetLibrary::SaveAsset(BlendSpacePath, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("animation"), AnimPath);
	Data->SetNumberField(TEXT("value_x"), ValueX);
	Data->SetNumberField(TEXT("value_y"), ValueY);
	Data->SetNumberField(TEXT("total_samples"), BlendSpace->GetBlendSamples().Num());
	return MakeResponse(true, Data);
}

// ===== ANIM SEQUENCE READING =====

FString FMCPServer::HandleReadAnimSequence(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AnimPath;
	if (!Params->TryGetStringField(TEXT("path"), AnimPath))
		return MakeError(TEXT("path required"));

	UAnimSequenceBase* AnimSeq = LoadObject<UAnimSequenceBase>(nullptr, *AnimPath);
	if (!AnimSeq)
		return MakeError(FString::Printf(TEXT("Animation not found: %s"), *AnimPath));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), AnimPath);
	Data->SetStringField(TEXT("class"), AnimSeq->GetClass()->GetName());
	Data->SetNumberField(TEXT("length"), AnimSeq->GetPlayLength());
	Data->SetStringField(TEXT("skeleton"), AnimSeq->GetSkeleton() ? AnimSeq->GetSkeleton()->GetPathName() : TEXT("None"));

	// Notifies
	TArray<TSharedPtr<FJsonValue>> Notifies;
	for (const FAnimNotifyEvent& Notify : AnimSeq->Notifies)
	{
		TSharedPtr<FJsonObject> N = MakeShared<FJsonObject>();
		N->SetStringField(TEXT("name"), Notify.NotifyName.ToString());
		N->SetNumberField(TEXT("time"), Notify.GetTriggerTime());
		if (Notify.Notify)
			N->SetStringField(TEXT("notify_class"), Notify.Notify->GetClass()->GetName());
		if (Notify.NotifyStateClass.Get())
			N->SetStringField(TEXT("state_class"), Notify.NotifyStateClass.Get()->GetClass()->GetName());
		Notifies.Add(MakeShared<FJsonValueObject>(N));
	}
	Data->SetArrayField(TEXT("notifies"), Notifies);
	Data->SetNumberField(TEXT("notify_count"), Notifies.Num());

	// Rate scale
	if (UAnimSequence* Seq = Cast<UAnimSequence>(AnimSeq))
	{
		Data->SetNumberField(TEXT("rate_scale"), Seq->RateScale);
		Data->SetNumberField(TEXT("num_frames"), Seq->GetNumberOfSampledKeys());
	}

	return MakeResponse(true, Data);
}

// Sets a property on every AnimNotify/AnimNotifyState instance of a given
// class found within AnimSequence/AnimMontage assets under path_filter -
// the Notifies array's element objects are individually-editable per-anim
// instances, and the raw Notifies property is blocked from Python's generic
// reflection (see AGENTS.md's "protected and cannot be read" family of
// gotchas), so this can only be done from native C++. Built specifically to
// bulk-wire UALSAnimNotifyFootstep::HitDataTable across every ALS-Community
// locomotion animation in one call instead of needing a per-notify tool.
FString FMCPServer::HandleSetAnimNotifyProperty(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString PathFilter, NotifyClassName, PropertyName, PropertyValue;
	if (!Params->TryGetStringField(TEXT("path_filter"), PathFilter))
		return MakeError(TEXT("path_filter required (e.g. /ALSV4_CPP)"));
	if (!Params->TryGetStringField(TEXT("notify_class_name"), NotifyClassName))
		return MakeError(TEXT("notify_class_name required (e.g. ALSAnimNotifyFootstep)"));
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
		return MakeError(TEXT("property_name required"));
	if (!Params->TryGetStringField(TEXT("property_value"), PropertyValue))
		return MakeError(TEXT("property_value required"));

	bool bRecursive = true;
	Params->TryGetBoolField(TEXT("recursive"), bRecursive);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> FolderAssets;
	AssetRegistry.GetAssetsByPath(FName(*PathFilter), FolderAssets, bRecursive);

	TArray<TSharedPtr<FJsonValue>> TouchedAssets;
	int32 NotifyCount = 0;

	for (const FAssetData& AssetDataEntry : FolderAssets)
	{
		UObject* Asset = AssetDataEntry.GetAsset();
		UAnimSequenceBase* AnimSeq = Cast<UAnimSequenceBase>(Asset);
		if (!AnimSeq)
			continue;

		bool bModifiedThisAsset = false;

		for (FAnimNotifyEvent& NotifyEvent : AnimSeq->Notifies)
		{
			UObject* NotifyObject = nullptr;
			if (NotifyEvent.Notify && NotifyEvent.Notify->GetClass()->GetName() == NotifyClassName)
			{
				NotifyObject = NotifyEvent.Notify;
			}
			else if (NotifyEvent.NotifyStateClass.Get() && NotifyEvent.NotifyStateClass.Get()->GetClass()->GetName() == NotifyClassName)
			{
				NotifyObject = NotifyEvent.NotifyStateClass.Get();
			}

			if (!NotifyObject)
				continue;

			FProperty* Property = NotifyObject->GetClass()->FindPropertyByName(*PropertyName);
			if (!Property)
				continue;

			if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
			{
				UObject* Loaded = LoadObject<UObject>(nullptr, *PropertyValue);
				UClass* ResolvedClass = Cast<UClass>(Loaded);
				if (!ResolvedClass)
				{
					if (UBlueprint* ReferencedBlueprint = Cast<UBlueprint>(Loaded))
					{
						ResolvedClass = ReferencedBlueprint->GeneratedClass;
					}
				}
				if (!ResolvedClass)
					continue;
				ClassProp->SetObjectPropertyValue(ClassProp->ContainerPtrToValuePtr<void>(NotifyObject), ResolvedClass);
			}
			else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
			{
				UObject* ReferencedObject = LoadObject<UObject>(nullptr, *PropertyValue);
				if (!ReferencedObject)
					continue;
				ObjProp->SetObjectPropertyValue(ObjProp->ContainerPtrToValuePtr<void>(NotifyObject), ReferencedObject);
			}
			else
			{
				void* ValuePtr = Property->ContainerPtrToValuePtr<void>(NotifyObject);
				if (!Property->ImportText_Direct(*PropertyValue, ValuePtr, NotifyObject, PPF_None))
					continue;
			}

			NotifyCount++;
			bModifiedThisAsset = true;
		}

		if (bModifiedThisAsset)
		{
			AnimSeq->MarkPackageDirty();
			UEditorAssetLibrary::SaveAsset(AssetDataEntry.GetObjectPathString(), false);
			TouchedAssets.Add(MakeShared<FJsonValueString>(AssetDataEntry.GetObjectPathString()));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("notify_count"), NotifyCount);
	Data->SetArrayField(TEXT("touched_assets"), TouchedAssets);
	return MakeResponse(true, Data);
}

// ===== ANIM BLUEPRINT CREATION =====

FString FMCPServer::HandleCreateAnimBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath, AssetName, SkeletonPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required (e.g. /Game/Blueprints/Animations)"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));
	if (!Params->TryGetStringField(TEXT("skeleton_path"), SkeletonPath))
		return MakeError(TEXT("skeleton_path required (path to USkeleton asset)"));

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
		return MakeError(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
	Factory->BlueprintType = BPTYPE_Normal;
	Factory->TargetSkeleton = Skeleton;
	Factory->ParentClass = UAnimInstance::StaticClass();

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, UAnimBlueprint::StaticClass(), Factory);
	if (!NewAsset)
		return MakeError(TEXT("Failed to create AnimBlueprint"));

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("skeleton"), SkeletonPath);
	return MakeResponse(true, Data);
}

// Creates a Post Process Anim Blueprint with ModifyBone nodes for spine and head.
// Assign this ABP to the mesh and update its FAnimNode_ModifyBone rotations from
// runtime code. Direct node updates avoid exposed-pin evaluation overwriting them.
FString FMCPServer::HandleSetupFpSpinePitchAbp(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath, AssetName, SkeletonPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));
	if (!Params->TryGetStringField(TEXT("skeleton_path"), SkeletonPath))
		return MakeError(TEXT("skeleton_path required"));

	FString BoneName = TEXT("spine_01");
	Params->TryGetStringField(TEXT("bone_name"), BoneName);

	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
		return MakeError(FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath));

	// ------ 0. Delete existing asset if present (so we can recreate cleanly) ------
	FString FullAssetPath = AssetPath / AssetName;
	if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
	{
		UEditorAssetLibrary::DeleteAsset(FullAssetPath);
	}

	// ------ 1. Create the AnimBlueprint ------
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
	Factory->BlueprintType = BPTYPE_Normal;
	Factory->TargetSkeleton = Skeleton;
	Factory->ParentClass = UAnimInstance::StaticClass();

	UAnimBlueprint* ABP = Cast<UAnimBlueprint>(
		AssetTools.CreateAsset(AssetName, AssetPath, UAnimBlueprint::StaticClass(), Factory));
	if (!ABP)
		return MakeError(TEXT("Failed to create AnimBlueprint"));

	// ------ 2. Find the AnimGraph ------
	UEdGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : ABP->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == TEXT("AnimGraph"))
		{
			AnimGraph = Graph;
			break;
		}
	}
	if (!AnimGraph)
		return MakeError(TEXT("AnimGraph not found in newly created ABP"));

	// ------ 4. Find existing Root node ------
	UAnimGraphNode_Root* RootNode = nullptr;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (UAnimGraphNode_Root* Root = Cast<UAnimGraphNode_Root>(Node))
		{
			RootNode = Root;
			break;
		}
	}
	if (!RootNode)
		return MakeError(TEXT("AnimGraphNode_Root not found"));

	// ------ 4.5. Create LinkedInputPose node (provides parent ABP's animated pose) ------
	// This is the correct way to get the parent ABP's output in a post-process ABP.
	// Without this, ModifyBone.ComponentPose is unconnected → reference pose → T-pose.
	UAnimGraphNode_LinkedInputPose* LinkedInputPoseNode = NewObject<UAnimGraphNode_LinkedInputPose>(AnimGraph);
	LinkedInputPoseNode->CreateNewGuid();
	AnimGraph->AddNode(LinkedInputPoseNode, false);
	LinkedInputPoseNode->NodePosX = RootNode->NodePosX - 700;
	LinkedInputPoseNode->NodePosY = RootNode->NodePosY;
	LinkedInputPoseNode->AllocateDefaultPins();

	// ------ 5. Create ModifyBone node for spine (BoneName param, typically spine_01) ------
	auto MakeModifyBoneNode = [&](const FString& Bone, int32 PosXOffset) -> UAnimGraphNode_ModifyBone*
	{
		UAnimGraphNode_ModifyBone* N = NewObject<UAnimGraphNode_ModifyBone>(AnimGraph);
		N->CreateNewGuid();
		N->Node.BoneToModify.BoneName = FName(*Bone);
		N->Node.RotationMode = EBoneModificationMode::BMM_Additive;
		N->Node.RotationSpace = EBoneControlSpace::BCS_WorldSpace; // World space: Pitch = forward tilt
		N->Node.TranslationMode = EBoneModificationMode::BMM_Ignore;
		N->Node.ScaleMode = EBoneModificationMode::BMM_Ignore;
		N->Node.Alpha = 1.0f;
		AnimGraph->AddNode(N, false);
		N->NodePosX = RootNode->NodePosX + PosXOffset;
		N->NodePosY = RootNode->NodePosY;
		N->AllocateDefaultPins();
		return N;
	};

	UAnimGraphNode_ModifyBone* SpineNode = MakeModifyBoneNode(BoneName, -500);  // spine_01
	UAnimGraphNode_ModifyBone* HeadNode  = MakeModifyBoneNode(TEXT("head"),  -300);  // head

	// ------ 6. LocalToComponentSpace: LinkedInputPose[local] → L2CS → SpineNode[component] ------
	UAnimGraphNode_LocalToComponentSpace* LocalToCSNode = NewObject<UAnimGraphNode_LocalToComponentSpace>(AnimGraph);
	LocalToCSNode->CreateNewGuid();
	AnimGraph->AddNode(LocalToCSNode, false);
	LocalToCSNode->NodePosX = RootNode->NodePosX - 700;
	LocalToCSNode->NodePosY = RootNode->NodePosY;
	LocalToCSNode->AllocateDefaultPins();

	UEdGraphPin* LinkedPoseOutPin   = LinkedInputPoseNode->FindPin(TEXT("Pose"), EGPD_Output);
	UEdGraphPin* LocalToCSInputPin  = LocalToCSNode->FindPin(TEXT("LocalPose"));
	UEdGraphPin* LocalToCSOutputPin = LocalToCSNode->FindPin(TEXT("ComponentPose"));
	UEdGraphPin* SpineCompPosePin   = SpineNode->FindPin(TEXT("ComponentPose"));

	bool bLinkedPoseConnected = false;
	if (LinkedPoseOutPin && LocalToCSInputPin)  { LinkedPoseOutPin->MakeLinkTo(LocalToCSInputPin);  bLinkedPoseConnected = true; }
	if (LocalToCSOutputPin && SpineCompPosePin) { LocalToCSOutputPin->MakeLinkTo(SpineCompPosePin); }

	// Chain: SpineNode.Pose → HeadNode.ComponentPose
	UEdGraphPin* SpineOutPin      = SpineNode->FindPin(TEXT("Pose"));
	UEdGraphPin* HeadCompPosePin  = HeadNode->FindPin(TEXT("ComponentPose"));
	if (SpineOutPin && HeadCompPosePin) { SpineOutPin->MakeLinkTo(HeadCompPosePin); }

	// ------ 7. ComponentToLocalSpace: HeadNode[component] → CS2L → Root[local] ------
	UAnimGraphNode_ComponentToLocalSpace* CSToLocalNode = NewObject<UAnimGraphNode_ComponentToLocalSpace>(AnimGraph);
	CSToLocalNode->CreateNewGuid();
	AnimGraph->AddNode(CSToLocalNode, false);
	CSToLocalNode->NodePosX = RootNode->NodePosX - 100;
	CSToLocalNode->NodePosY = RootNode->NodePosY;
	CSToLocalNode->AllocateDefaultPins();

	UEdGraphPin* HeadOutPin         = HeadNode->FindPin(TEXT("Pose"));
	UEdGraphPin* CSToLocalInputPin  = CSToLocalNode->FindPin(TEXT("ComponentPose"));
	UEdGraphPin* CSToLocalOutputPin = CSToLocalNode->FindPin(TEXT("Pose"), EGPD_Output);
	UEdGraphPin* RootInputPin       = RootNode->FindPin(TEXT("Result"));

	if (HeadOutPin && CSToLocalInputPin)    { HeadOutPin->MakeLinkTo(CSToLocalInputPin);    }
	if (CSToLocalOutputPin && RootInputPin) { CSToLocalOutputPin->MakeLinkTo(RootInputPin); }

	// ------ 9. Compile and save ------
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ABP);
	FKismetEditorUtilities::CompileBlueprint(ABP, EBlueprintCompileOptions::SkipGarbageCollection);
	UEditorAssetLibrary::SaveAsset(ABP->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), ABP->GetPathName());
	Data->SetStringField(TEXT("spine_bone"), BoneName);
	Data->SetBoolField(TEXT("linked_pose_connected"), bLinkedPoseConnected);
	Data->SetBoolField(TEXT("spine_chain_ok"), LocalToCSOutputPin != nullptr && SpineCompPosePin != nullptr && SpineOutPin != nullptr && HeadCompPosePin != nullptr);
	Data->SetBoolField(TEXT("head_to_root_ok"), HeadOutPin != nullptr && CSToLocalInputPin != nullptr && CSToLocalOutputPin != nullptr && RootInputPin != nullptr);
	return MakeResponse(true, Data);
}

// Retargets an animation asset (AnimSequence, AnimMontage, etc.) from its
// current Skeleton onto a different one by bone name, using the same
// underlying EditorAnimUtils::RetargetAnimations() the classic "Retarget
// Skeleton" right-click tool in the Content Browser calls. This is plain
// C++ (namespace free functions in the UnrealEd module), not exposed to
// Blueprint or Python at all - there is no scriptable way to do this
// without a tool like this one. Only works well when the two skeletons
// share compatible bone names/hierarchy (no IK Rig retargeting math is
// applied here, just remapping which Skeleton asset the animation data is
// interpreted against) - for skeletons with different proportions or bone
// naming, a proper IK Rig/IK Retargeter setup is needed instead, which
// this does not attempt.
FString FMCPServer::HandleRetargetAnimAsset(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString SourceAssetPath, TargetSkeletonPath, DestFolder;
	if (!Params->TryGetStringField(TEXT("source_asset_path"), SourceAssetPath))
		return MakeError(TEXT("source_asset_path required"));
	if (!Params->TryGetStringField(TEXT("target_skeleton_path"), TargetSkeletonPath))
		return MakeError(TEXT("target_skeleton_path required"));
	if (!Params->TryGetStringField(TEXT("dest_folder"), DestFolder))
		return MakeError(TEXT("dest_folder required (e.g. '/Game/ALSHost/Animations')"));

	FString Suffix = Params->HasField(TEXT("name_suffix")) ? Params->GetStringField(TEXT("name_suffix")) : TEXT("");
	FString Prefix = Params->HasField(TEXT("name_prefix")) ? Params->GetStringField(TEXT("name_prefix")) : TEXT("");
	bool bConvertSpace = true;
	Params->TryGetBoolField(TEXT("convert_space"), bConvertSpace);

	UAnimationAsset* SourceAsset = LoadObject<UAnimationAsset>(nullptr, *SourceAssetPath);
	if (!SourceAsset)
		return MakeError(FString::Printf(TEXT("Animation asset not found: %s"), *SourceAssetPath));

	USkeleton* NewSkeleton = LoadObject<USkeleton>(nullptr, *TargetSkeletonPath);
	if (!NewSkeleton)
		return MakeError(FString::Printf(TEXT("Target skeleton not found: %s"), *TargetSkeletonPath));

	USkeleton* OldSkeleton = SourceAsset->GetSkeleton();
	if (!OldSkeleton)
		return MakeError(TEXT("Source asset has no Skeleton set"));

	if (OldSkeleton == NewSkeleton)
		return MakeError(TEXT("Source asset already targets the requested skeleton"));

	EditorAnimUtils::FNameDuplicationRule NameRule;
	NameRule.FolderPath = DestFolder;
	NameRule.Prefix = Prefix;
	NameRule.Suffix = Suffix;

	TArray<TWeakObjectPtr<UObject>> AssetsToRetarget;
	AssetsToRetarget.Add(SourceAsset);

	UObject* RetargetedAsset = EditorAnimUtils::RetargetAnimations(
		OldSkeleton, NewSkeleton, AssetsToRetarget,
		/*bRetargetReferredAssets=*/true, &NameRule, bConvertSpace);

	if (!RetargetedAsset)
		return MakeError(TEXT("RetargetAnimations returned no asset - check the source/target skeletons share compatible bone names"));

	UEditorAssetLibrary::SaveAsset(RetargetedAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("source_path"), SourceAssetPath);
	Data->SetStringField(TEXT("retargeted_path"), RetargetedAsset->GetPathName());
	Data->SetStringField(TEXT("target_skeleton"), TargetSkeletonPath);
	return MakeResponse(true, Data);
}

static TSharedPtr<FJsonObject> SerializeTransform(const FTransform& Transform)
{
	const FVector Loc = Transform.GetLocation();
	const FRotator Rot = Transform.Rotator();
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
	LocObj->SetNumberField(TEXT("x"), Loc.X);
	LocObj->SetNumberField(TEXT("y"), Loc.Y);
	LocObj->SetNumberField(TEXT("z"), Loc.Z);
	TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
	RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
	RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
	RotObj->SetNumberField(TEXT("roll"), Rot.Roll);
	Obj->SetObjectField(TEXT("location"), LocObj);
	Obj->SetObjectField(TEXT("rotation"), RotObj);
	return Obj;
}

// Compares a bone's component-space transform between two animations at two
// given times - built to answer "what offset would make this weapon's grip
// bone match a known-good animation's grip bone", since our HeldObjectRoot
// offset is a single constant applied relative to a fixed virtual bone that
// itself rigidly follows this exact real bone (hand_r) every frame. If the
// mismatch is a genuinely constant offset (not a per-frame drift), "delta"
// below is directly usable as ReloadHeldObjectLocationOffset/RotationOffset.
FString FMCPServer::HandleCompareAnimBonePose(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AnimPathA, AnimPathB, BoneName;
	double TimeA = 0.0, TimeB = 0.0;
	if (!Params->TryGetStringField(TEXT("anim_path_a"), AnimPathA))
		return MakeError(TEXT("anim_path_a required"));
	if (!Params->TryGetStringField(TEXT("anim_path_b"), AnimPathB))
		return MakeError(TEXT("anim_path_b required"));
	if (!Params->TryGetNumberField(TEXT("time_a"), TimeA))
		return MakeError(TEXT("time_a required (seconds)"));
	if (!Params->TryGetNumberField(TEXT("time_b"), TimeB))
		return MakeError(TEXT("time_b required (seconds)"));
	if (!Params->TryGetStringField(TEXT("bone_name"), BoneName))
		BoneName = TEXT("hand_r");

	UAnimSequenceBase* AnimA = LoadObject<UAnimSequenceBase>(nullptr, *AnimPathA);
	if (!AnimA)
		return MakeError(FString::Printf(TEXT("Animation not found: %s"), *AnimPathA));

	UAnimSequenceBase* AnimB = LoadObject<UAnimSequenceBase>(nullptr, *AnimPathB);
	if (!AnimB)
		return MakeError(FString::Printf(TEXT("Animation not found: %s"), *AnimPathB));

	FAnimPoseEvaluationOptions EvalOptions;
	FAnimPose PoseA, PoseB;
	UAnimPoseExtensions::GetAnimPoseAtTime(AnimA, TimeA, EvalOptions, PoseA);
	UAnimPoseExtensions::GetAnimPoseAtTime(AnimB, TimeB, EvalOptions, PoseB);

	TArray<FName> BoneNamesA, BoneNamesB;
	UAnimPoseExtensions::GetBoneNames(PoseA, BoneNamesA);
	UAnimPoseExtensions::GetBoneNames(PoseB, BoneNamesB);
	if (!BoneNamesA.Contains(FName(*BoneName)))
		return MakeError(FString::Printf(TEXT("Bone '%s' not found in %s"), *BoneName, *AnimPathA));
	if (!BoneNamesB.Contains(FName(*BoneName)))
		return MakeError(FString::Printf(TEXT("Bone '%s' not found in %s"), *BoneName, *AnimPathB));

	const FTransform TransformA = UAnimPoseExtensions::GetBonePose(PoseA, FName(*BoneName), EAnimPoseSpaces::World);
	const FTransform TransformB = UAnimPoseExtensions::GetBonePose(PoseB, FName(*BoneName), EAnimPoseSpaces::World);

	// B relative to A: the transform that, applied on top of A, produces B.
	const FTransform DeltaBRelativeToA = TransformB.GetRelativeTransform(TransformA);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("bone_name"), BoneName);
	Data->SetObjectField(TEXT("pose_a"), SerializeTransform(TransformA));
	Data->SetObjectField(TEXT("pose_b"), SerializeTransform(TransformB));
	Data->SetObjectField(TEXT("delta_b_relative_to_a"), SerializeTransform(DeltaBRelativeToA));
	return MakeResponse(true, Data);
}

// Authors a float curve (constant or keyed) directly onto an AnimSequence -
// e.g. ALS's Enable_HandIK_L/Layering_Arm_L curves, which gate the AnimGraph's
// support-hand IK (see HandIK graph in ALS_AnimBP) but are never present on
// animations retargeted from a project that doesn't use ALS's curve
// conventions, silently leaving that IK system inactive. Wraps
// UAnimationBlueprintLibrary::AddCurve/AddFloatCurveKeys - the curve name
// must already exist in the target Skeleton's curve metadata (check via
// inspect_asset on the Skeleton) for the AnimGraph's GetCurveValue lookups to
// actually see it.
FString FMCPServer::HandleSetAnimCurveKeys(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AnimPath, CurveName;
	if (!Params->TryGetStringField(TEXT("anim_path"), AnimPath))
		return MakeError(TEXT("anim_path required"));
	if (!Params->TryGetStringField(TEXT("curve_name"), CurveName))
		return MakeError(TEXT("curve_name required"));

	const TArray<TSharedPtr<FJsonValue>>* TimesArray = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* ValuesArray = nullptr;
	if (!Params->TryGetArrayField(TEXT("times"), TimesArray) || !TimesArray)
		return MakeError(TEXT("times required (array of floats, seconds)"));
	if (!Params->TryGetArrayField(TEXT("values"), ValuesArray) || !ValuesArray)
		return MakeError(TEXT("values required (array of floats, same length as times)"));
	if (TimesArray->Num() != ValuesArray->Num() || TimesArray->Num() == 0)
		return MakeError(TEXT("times and values must be non-empty and the same length"));

	UAnimSequenceBase* Anim = LoadObject<UAnimSequenceBase>(nullptr, *AnimPath);
	if (!Anim)
		return MakeError(FString::Printf(TEXT("Animation not found: %s"), *AnimPath));

	TArray<float> Times, Values;
	for (const TSharedPtr<FJsonValue>& V : *TimesArray) Times.Add(static_cast<float>(V->AsNumber()));
	for (const TSharedPtr<FJsonValue>& V : *ValuesArray) Values.Add(static_cast<float>(V->AsNumber()));

	const FName CurveFName(*CurveName);
	UAnimationBlueprintLibrary::AddCurve(Anim, CurveFName, ERawCurveTrackTypes::RCT_Float, false);
	UAnimationBlueprintLibrary::AddFloatCurveKeys(Anim, CurveFName, Times, Values);

	UEditorAssetLibrary::SaveAsset(AnimPath, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("anim_path"), AnimPath);
	Data->SetStringField(TEXT("curve_name"), CurveName);
	Data->SetNumberField(TEXT("key_count"), Times.Num());
	return MakeResponse(true, Data);
}

// ===== ANIMGRAPH STATE MACHINES =====
//
// AnimGraph state machine sub-graphs (the states/transitions nested inside an
// AnimGraphNode_StateMachine node) are invisible to read_function_graphs/
// read_event_graph_detailed - those only walk Blueprint->FunctionGraphs/
// UbergraphPages directly, and a state machine's UAnimationStateMachineGraph
// is reached only via the containing AnimGraphNode_StateMachine's own
// EditorStateMachineGraph property (not a generically-reflected sub-graph
// list). See AGENTS.md's "AnimGraph state machines aren't in FunctionGraphs/
// UbergraphPages" entry. Built to inspect ALS_AnimBP's "Overlay States"
// machine (in the OverlayLayer anim layer function) when adding a new
// EALSOverlayState value (Sword) that needs to reuse an existing state's
// (Torch's) pose - no way to see what's actually wired there otherwise.

static UAnimGraphNode_StateMachine* FindStateMachineNode(UEdGraph* ContainerGraph, const FString& NodeNameFilter)
{
	for (UEdGraphNode* Node : ContainerGraph->Nodes)
	{
		UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node);
		if (!SMNode) continue;
		if (NodeNameFilter.IsEmpty()) return SMNode;
		if (SMNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Contains(NodeNameFilter)) return SMNode;
		if (SMNode->EditorStateMachineGraph && SMNode->EditorStateMachineGraph->GetName().Contains(NodeNameFilter)) return SMNode;
	}
	return nullptr;
}

// Briefly summarizes a transition rule's BoundGraph (a small K2 graph
// returning a bool) - specifically pulls out any Equal/NotEqual comparison
// against a literal byte/enum value, which is ALS's actual pattern for
// "OverlayState == SomeValue" gating. Falls back to a generic node/pin dump
// for anything more complex so nothing is silently hidden.
static TSharedPtr<FJsonObject> SummarizeTransitionRule(UEdGraph* RuleGraph)
{
	TSharedPtr<FJsonObject> RuleObj = MakeShared<FJsonObject>();
	if (!RuleGraph)
	{
		RuleObj->SetBoolField(TEXT("has_rule_graph"), false);
		return RuleObj;
	}
	RuleObj->SetBoolField(TEXT("has_rule_graph"), true);

	TArray<TSharedPtr<FJsonValue>> NodesArray;
	for (UEdGraphNode* RNode : RuleGraph->Nodes)
	{
		if (!RNode) continue;
		TSharedPtr<FJsonObject> RN = MakeShared<FJsonObject>();
		RN->SetStringField(TEXT("node_guid"), RNode->NodeGuid.ToString());
		RN->SetStringField(TEXT("class"), RNode->GetClass()->GetName());
		RN->SetStringField(TEXT("title"), RNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

		if (UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(RNode))
		{
			RN->SetStringField(TEXT("function_name"), FuncNode->GetFunctionName().ToString());
		}

		TArray<TSharedPtr<FJsonValue>> PinsArray;
		for (UEdGraphPin* Pin : RNode->Pins)
		{
			if (!Pin) continue;
			TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
			PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
			PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
			if (!Pin->DefaultValue.IsEmpty()) PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
			if (Pin->DefaultObject) PinObj->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
			PinObj->SetBoolField(TEXT("is_linked"), Pin->LinkedTo.Num() > 0);
			TArray<TSharedPtr<FJsonValue>> LinkedFrom;
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (!Linked || !Linked->GetOwningNode()) continue;
				TSharedPtr<FJsonObject> L = MakeShared<FJsonObject>();
				L->SetStringField(TEXT("node_guid"), Linked->GetOwningNode()->NodeGuid.ToString());
				L->SetStringField(TEXT("pin_name"), Linked->PinName.ToString());
				LinkedFrom.Add(MakeShared<FJsonValueObject>(L));
			}
			if (LinkedFrom.Num() > 0) PinObj->SetArrayField(TEXT("linked_to"), LinkedFrom);
			PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
		}
		RN->SetArrayField(TEXT("pins"), PinsArray);
		NodesArray.Add(MakeShared<FJsonValueObject>(RN));
	}
	RuleObj->SetArrayField(TEXT("nodes"), NodesArray);
	return RuleObj;
}

// Reads a state machine's states and transitions, since no existing tool can
// see past the containing AnimGraphNode_StateMachine node (see comment
// above). blueprint_path + graph_name locate the function graph holding the
// state machine node (e.g. an Anim Layer function like "OverlayLayer");
// state_machine_node_name matches against the node's title or its bound
// graph's own name (e.g. "Overlay States").
FString FMCPServer::HandleReadStateMachine(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString Path, GraphName, NodeNameFilter;
	if (!Params->TryGetStringField(TEXT("blueprint_path"), Path))
		return MakeError(TEXT("blueprint_path required"));
	if (!Params->TryGetStringField(TEXT("graph_name"), GraphName))
		return MakeError(TEXT("graph_name required (the function graph containing the state machine node, e.g. an Anim Layer function name)"));
	Params->TryGetStringField(TEXT("state_machine_node_name"), NodeNameFilter);

	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);
	if (!Blueprint)
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));

	UEdGraph* ContainerGraph = nullptr;
	for (UEdGraph* G : Blueprint->FunctionGraphs)
	{
		if (G && G->GetName() == GraphName) { ContainerGraph = G; break; }
	}
	if (!ContainerGraph)
	{
		for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
		{
			for (UEdGraph* G : Interface.Graphs)
			{
				if (G && G->GetName() == GraphName) { ContainerGraph = G; break; }
			}
			if (ContainerGraph) break;
		}
	}
	if (!ContainerGraph)
		return MakeError(FString::Printf(TEXT("Function graph not found: %s"), *GraphName));

	UAnimGraphNode_StateMachine* SMNode = FindStateMachineNode(ContainerGraph, NodeNameFilter);
	if (!SMNode)
		return MakeError(FString::Printf(TEXT("State machine node not found in graph '%s' (filter: '%s')"), *GraphName, *NodeNameFilter));
	if (!SMNode->EditorStateMachineGraph)
		return MakeError(TEXT("State machine node has no EditorStateMachineGraph"));

	UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;

	FString EntryStateName;
	TArray<TSharedPtr<FJsonValue>> StatesArray;
	TArray<TSharedPtr<FJsonValue>> TransitionsArray;

	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (!Node) continue;

		if (UAnimStateEntryNode* EntryNode = Cast<UAnimStateEntryNode>(Node))
		{
			for (UEdGraphPin* Pin : EntryNode->Pins)
			{
				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (UAnimStateNodeBase* StateBase = Cast<UAnimStateNodeBase>(Linked->GetOwningNode()))
					{
						EntryStateName = StateBase->GetStateName();
					}
				}
			}
		}
		else if (UAnimStateTransitionNode* Trans = Cast<UAnimStateTransitionNode>(Node))
		{
			TSharedPtr<FJsonObject> T = MakeShared<FJsonObject>();
			T->SetStringField(TEXT("node_guid"), Trans->NodeGuid.ToString());
			UAnimStateNodeBase* PrevState = Trans->GetPreviousState();
			UAnimStateNodeBase* NextState = Trans->GetNextState();
			T->SetStringField(TEXT("from_state"), PrevState ? PrevState->GetStateName() : FString());
			T->SetStringField(TEXT("to_state"), NextState ? NextState->GetStateName() : FString());
			T->SetStringField(TEXT("from_state_guid"), PrevState ? Cast<UEdGraphNode>(PrevState)->NodeGuid.ToString() : FString());
			T->SetStringField(TEXT("to_state_guid"), NextState ? Cast<UEdGraphNode>(NextState)->NodeGuid.ToString() : FString());
			T->SetObjectField(TEXT("rule"), SummarizeTransitionRule(Trans->BoundGraph));
			TransitionsArray.Add(MakeShared<FJsonValueObject>(T));
		}
		else if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
		{
			TSharedPtr<FJsonObject> S = MakeShared<FJsonObject>();
			S->SetStringField(TEXT("name"), StateNode->GetStateName());
			S->SetStringField(TEXT("node_guid"), StateNode->NodeGuid.ToString());
			StatesArray.Add(MakeShared<FJsonValueObject>(S));
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("state_machine_name"), SMGraph->GetName());
	Data->SetStringField(TEXT("entry_state"), EntryStateName);
	Data->SetArrayField(TEXT("states"), StatesArray);
	Data->SetNumberField(TEXT("state_count"), StatesArray.Num());
	Data->SetArrayField(TEXT("transitions"), TransitionsArray);
	Data->SetNumberField(TEXT("transition_count"), TransitionsArray.Num());
	return MakeResponse(true, Data);
}

// Clones an existing transition (its BoundGraph rule graph included, via the
// same ExportNodesToText/ImportNodesFromText path Task 3's duplicate_node op
// uses for regular graph nodes - see AGENTS.md) between two named states in
// the same state machine, keeping the SOURCE state the same as the original
// and pointing the new copy's target at a different existing state. Used to
// give a brand-new enum value (e.g. Sword) an entry transition into an
// existing state (e.g. Torch) that mirrors whatever condition already lets a
// known-working value (e.g. the Torch value itself, or whatever the existing
// From state's transition into Torch already checks) trigger - the actual
// enum literal inside the cloned rule graph still needs a follow-up
// set_pin_default call once this returns the new rule graph's node info, the
// same way Task 3 used set_pin_default after duplicate_node.
FString FMCPServer::HandleDuplicateStateTransition(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString Path, GraphName, NodeNameFilter, SourceTransitionGuid, NewTargetStateName;
	if (!Params->TryGetStringField(TEXT("blueprint_path"), Path))
		return MakeError(TEXT("blueprint_path required"));
	if (!Params->TryGetStringField(TEXT("graph_name"), GraphName))
		return MakeError(TEXT("graph_name required"));
	Params->TryGetStringField(TEXT("state_machine_node_name"), NodeNameFilter);
	if (!Params->TryGetStringField(TEXT("source_transition_guid"), SourceTransitionGuid))
		return MakeError(TEXT("source_transition_guid required (from read_state_machine's transitions[].node_guid)"));
	if (!Params->TryGetStringField(TEXT("new_target_state_name"), NewTargetStateName))
		return MakeError(TEXT("new_target_state_name required (an existing state's name to point the cloned transition's target at; the source stays the same)"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);
	if (!Blueprint)
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));

	UEdGraph* ContainerGraph = nullptr;
	for (UEdGraph* G : Blueprint->FunctionGraphs)
	{
		if (G && G->GetName() == GraphName) { ContainerGraph = G; break; }
	}
	if (!ContainerGraph)
		return MakeError(FString::Printf(TEXT("Function graph not found: %s"), *GraphName));

	UAnimGraphNode_StateMachine* SMNode = FindStateMachineNode(ContainerGraph, NodeNameFilter);
	if (!SMNode || !SMNode->EditorStateMachineGraph)
		return MakeError(TEXT("State machine node not found"));
	UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;

	FGuid SourceGuid;
	if (!FGuid::Parse(SourceTransitionGuid, SourceGuid))
		return MakeError(TEXT("source_transition_guid is not a valid GUID"));

	UAnimStateTransitionNode* SourceTransition = nullptr;
	UAnimStateNodeBase* NewTargetState = nullptr;
	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (UAnimStateTransitionNode* Trans = Cast<UAnimStateTransitionNode>(Node))
		{
			if (Trans->NodeGuid == SourceGuid) SourceTransition = Trans;
		}
		else if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
		{
			if (StateNode->GetStateName() == NewTargetStateName) NewTargetState = StateNode;
		}
	}
	if (!SourceTransition)
		return MakeError(FString::Printf(TEXT("Transition with guid %s not found"), *SourceTransitionGuid));
	if (!NewTargetState)
		return MakeError(FString::Printf(TEXT("Target state '%s' not found"), *NewTargetStateName));

	UAnimStateNodeBase* SourceOriginState = SourceTransition->GetPreviousState();
	if (!SourceOriginState)
		return MakeError(TEXT("Source transition has no origin state (GetPreviousState returned null)"));

	// Duplicate the transition node itself (copies PriorityOrder/CrossfadeDuration/
	// BlendMode/etc via the standard duplication path, not a field-by-field manual
	// copy) and give it a fresh identity before wiring it into the graph.
	UAnimStateTransitionNode* NewTransition = DuplicateObject<UAnimStateTransitionNode>(SourceTransition, SMGraph);
	if (!NewTransition)
		return MakeError(TEXT("Failed to duplicate transition node"));
	NewTransition->CreateNewGuid();
	SMGraph->AddNode(NewTransition, /*bFromUI=*/false, /*bSelectNewNode=*/false);
	NewTransition->NodePosX = SourceTransition->NodePosX + 40;
	NewTransition->NodePosY = SourceTransition->NodePosY + 60;

	// The duplicated BoundGraph rule is a straight UObject copy sharing no
	// UEdGraphPin links to the outside (transition rule graphs are small,
	// self-contained bool graphs with no external connections other than
	// their own Result node), but it still needs its own identity and to be
	// registered as this node's rule graph rather than aliasing the source's.
	if (SourceTransition->BoundGraph)
	{
		UEdGraph* NewRuleGraph = DuplicateObject<UEdGraph>(SourceTransition->BoundGraph, NewTransition);
		NewRuleGraph->Rename(nullptr, NewTransition, REN_DontCreateRedirectors);
		for (UEdGraphNode* RNode : NewRuleGraph->Nodes)
		{
			if (RNode) RNode->CreateNewGuid();
		}
		NewTransition->BoundGraph = NewRuleGraph;
	}

	// Clear the copied pin links (duplicated pins still point at the source
	// transition's neighbors) and reconnect: same origin state as the
	// source transition, new target state as requested.
	NewTransition->Pins.Empty();
	NewTransition->AllocateDefaultPins();
	NewTransition->CreateConnections(SourceOriginState, NewTargetState);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UEditorAssetLibrary::SaveAsset(Path, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("new_transition_guid"), NewTransition->NodeGuid.ToString());
	Data->SetStringField(TEXT("from_state"), SourceOriginState->GetStateName());
	Data->SetStringField(TEXT("to_state"), NewTargetState->GetStateName());
	Data->SetObjectField(TEXT("rule"), SummarizeTransitionRule(NewTransition->BoundGraph));
	return MakeResponse(true, Data);
}

// Widens an existing transition rule from "field X" to "field X OR field Y",
// where X and Y are two bool output pins on the SAME K2Node_BreakStruct node
// inside the rule's BoundGraph - this is exactly ALS's own pattern for
// gating a transition on one FALSOverlayState field (see read_state_machine's
// output: the "Break ALSOverlay State" node already exposes one bool output
// pin per struct field, including newly-added ones like "Sword_", left
// unlinked until a rule chooses to use it). Rewires whatever the existing
// field's pin was plugged into (directly into a Result node, or through a
// NOT node, or anything else) to instead come from a new BooleanOR node
// combining both fields, leaving everything else in the rule graph
// untouched. Built specifically to let a new EALSOverlayState value (e.g.
// Sword) share an existing state's (e.g. Torch's) both entry and exit
// transitions, rather than needing a whole new state/pose.
FString FMCPServer::HandleAddEnumOrConditionToTransitionRule(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString Path, GraphName, NodeNameFilter, TransitionGuidStr, ExistingPinName, NewPinName;
	if (!Params->TryGetStringField(TEXT("blueprint_path"), Path))
		return MakeError(TEXT("blueprint_path required"));
	if (!Params->TryGetStringField(TEXT("graph_name"), GraphName))
		return MakeError(TEXT("graph_name required"));
	Params->TryGetStringField(TEXT("state_machine_node_name"), NodeNameFilter);
	if (!Params->TryGetStringField(TEXT("transition_guid"), TransitionGuidStr))
		return MakeError(TEXT("transition_guid required (from read_state_machine's transitions[].node_guid)"));
	if (!Params->TryGetStringField(TEXT("existing_bool_pin_name"), ExistingPinName))
		return MakeError(TEXT("existing_bool_pin_name required (the BreakStruct output pin currently driving this rule, e.g. 'Torch_')"));
	if (!Params->TryGetStringField(TEXT("new_bool_pin_name"), NewPinName))
		return MakeError(TEXT("new_bool_pin_name required (the BreakStruct output pin to OR in, e.g. 'Sword_')"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);
	if (!Blueprint)
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));

	UEdGraph* ContainerGraph = nullptr;
	for (UEdGraph* G : Blueprint->FunctionGraphs)
	{
		if (G && G->GetName() == GraphName) { ContainerGraph = G; break; }
	}
	if (!ContainerGraph)
		return MakeError(FString::Printf(TEXT("Function graph not found: %s"), *GraphName));

	UAnimGraphNode_StateMachine* SMNode = FindStateMachineNode(ContainerGraph, NodeNameFilter);
	if (!SMNode || !SMNode->EditorStateMachineGraph)
		return MakeError(TEXT("State machine node not found"));
	UAnimationStateMachineGraph* SMGraph = SMNode->EditorStateMachineGraph;

	FGuid TransitionGuid;
	if (!FGuid::Parse(TransitionGuidStr, TransitionGuid))
		return MakeError(TEXT("transition_guid is not a valid GUID"));

	UAnimStateTransitionNode* Transition = nullptr;
	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (UAnimStateTransitionNode* Trans = Cast<UAnimStateTransitionNode>(Node))
		{
			if (Trans->NodeGuid == TransitionGuid) { Transition = Trans; break; }
		}
	}
	if (!Transition)
		return MakeError(FString::Printf(TEXT("Transition with guid %s not found"), *TransitionGuidStr));
	if (!Transition->BoundGraph)
		return MakeError(TEXT("Transition has no BoundGraph"));

	UEdGraph* RuleGraph = Transition->BoundGraph;

	UK2Node_BreakStruct* BreakNode = nullptr;
	for (UEdGraphNode* RNode : RuleGraph->Nodes)
	{
		if (UK2Node_BreakStruct* Candidate = Cast<UK2Node_BreakStruct>(RNode))
		{
			if (Candidate->FindPin(*ExistingPinName, EGPD_Output) && Candidate->FindPin(*NewPinName, EGPD_Output))
			{
				BreakNode = Candidate;
				break;
			}
		}
	}
	if (!BreakNode)
		return MakeError(FString::Printf(TEXT("No BreakStruct node in this rule graph has both pins '%s' and '%s'"), *ExistingPinName, *NewPinName));

	UEdGraphPin* ExistingPin = BreakNode->FindPin(*ExistingPinName, EGPD_Output);
	UEdGraphPin* NewPin = BreakNode->FindPin(*NewPinName, EGPD_Output);
	if (!ExistingPin || !NewPin)
		return MakeError(TEXT("Failed to resolve both pins after locating the BreakStruct node"));
	if (ExistingPin->LinkedTo.Num() != 1)
		return MakeError(FString::Printf(TEXT("Expected '%s' to have exactly one existing link, found %d"), *ExistingPinName, ExistingPin->LinkedTo.Num()));

	UEdGraphPin* ConsumerPin = ExistingPin->LinkedTo[0];
	if (!ConsumerPin || !ConsumerPin->GetOwningNode())
		return MakeError(TEXT("Existing pin's link target is invalid"));

	// Break the direct link before rewiring - MakeLinkTo on a pin that
	// already expects a single input silently replaces it, but breaking
	// first keeps this an explicit, easy-to-follow two-step rewire.
	ExistingPin->BreakLinkTo(ConsumerPin);

	UFunction* OrFunction = UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("BooleanOR"));
	if (!OrFunction)
		return MakeError(TEXT("UKismetMathLibrary::BooleanOR not found - engine API changed?"));

	UK2Node_CallFunction* OrNode = NewObject<UK2Node_CallFunction>(RuleGraph);
	OrNode->SetFromFunction(OrFunction);
	OrNode->CreateNewGuid();
	RuleGraph->AddNode(OrNode, /*bFromUI=*/false, /*bSelectNewNode=*/false);
	OrNode->NodePosX = BreakNode->NodePosX + 200;
	OrNode->NodePosY = BreakNode->NodePosY + 100;
	OrNode->AllocateDefaultPins();

	UEdGraphPin* OrPinA = OrNode->FindPin(TEXT("A"), EGPD_Input);
	UEdGraphPin* OrPinB = OrNode->FindPin(TEXT("B"), EGPD_Input);
	UEdGraphPin* OrPinReturn = OrNode->FindPin(TEXT("ReturnValue"), EGPD_Output);
	if (!OrPinA || !OrPinB || !OrPinReturn)
		return MakeError(TEXT("BooleanOR node did not expose the expected A/B/ReturnValue pins"));

	ExistingPin->MakeLinkTo(OrPinA);
	NewPin->MakeLinkTo(OrPinB);
	OrPinReturn->MakeLinkTo(ConsumerPin);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	UEditorAssetLibrary::SaveAsset(Path, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("or_node_guid"), OrNode->NodeGuid.ToString());
	Data->SetStringField(TEXT("existing_pin"), ExistingPinName);
	Data->SetStringField(TEXT("new_pin"), NewPinName);
	Data->SetStringField(TEXT("consumer_node_guid"), ConsumerPin->GetOwningNode()->NodeGuid.ToString());
	Data->SetStringField(TEXT("consumer_pin"), ConsumerPin->PinName.ToString());
	Data->SetObjectField(TEXT("rule"), SummarizeTransitionRule(RuleGraph));
	return MakeResponse(true, Data);
}
