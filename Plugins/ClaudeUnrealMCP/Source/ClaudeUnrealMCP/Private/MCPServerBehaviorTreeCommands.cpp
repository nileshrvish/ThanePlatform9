#include "MCPServer.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTreeFactory.h"
#include "BlackboardDataFactory.h"
#include "EditorAssetLibrary.h"

FString FMCPServer::HandleCreateBehaviorTree(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath, AssetName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required (e.g. '/Game/AI')"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UBehaviorTreeFactory* Factory = NewObject<UBehaviorTreeFactory>();

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, UBehaviorTree::StaticClass(), Factory);
	if (!NewAsset)
		return MakeError(TEXT("Failed to create behavior tree"));

	// Optionally set blackboard
	FString BlackboardPath;
	if (Params->TryGetStringField(TEXT("blackboard_path"), BlackboardPath))
	{
		UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
		if (BB)
		{
			UBehaviorTree* BT = Cast<UBehaviorTree>(NewAsset);
			if (BT) BT->BlackboardAsset = BB;
		}
	}

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("name"), AssetName);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleCreateBlackboard(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath, AssetName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UBlackboardDataFactory* Factory = NewObject<UBlackboardDataFactory>();

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, UBlackboardData::StaticClass(), Factory);
	if (!NewAsset)
		return MakeError(TEXT("Failed to create blackboard"));

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("name"), AssetName);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleAddBlackboardKey(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString BlackboardPath, KeyName, KeyType;
	if (!Params->TryGetStringField(TEXT("blackboard_path"), BlackboardPath))
		return MakeError(TEXT("blackboard_path required"));
	if (!Params->TryGetStringField(TEXT("key_name"), KeyName))
		return MakeError(TEXT("key_name required"));
	if (!Params->TryGetStringField(TEXT("key_type"), KeyType))
		return MakeError(TEXT("key_type required (Bool, Float, Int, String, Name, Object, Class, Enum, Vector, Rotator)"));

	UBlackboardData* BB = LoadObject<UBlackboardData>(nullptr, *BlackboardPath);
	if (!BB)
		return MakeError(FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardPath));

	// Resolve key type class
	static const TMap<FString, UClass*> KeyTypes = {
		{TEXT("Bool"), UBlackboardKeyType_Bool::StaticClass()},
		{TEXT("Float"), UBlackboardKeyType_Float::StaticClass()},
		{TEXT("Int"), UBlackboardKeyType_Int::StaticClass()},
		{TEXT("String"), UBlackboardKeyType_String::StaticClass()},
		{TEXT("Name"), UBlackboardKeyType_Name::StaticClass()},
		{TEXT("Object"), UBlackboardKeyType_Object::StaticClass()},
		{TEXT("Class"), UBlackboardKeyType_Class::StaticClass()},
		{TEXT("Enum"), UBlackboardKeyType_Enum::StaticClass()},
		{TEXT("Vector"), UBlackboardKeyType_Vector::StaticClass()},
		{TEXT("Rotator"), UBlackboardKeyType_Rotator::StaticClass()},
	};

	const UClass* const* TypeClass = KeyTypes.Find(KeyType);
	if (!TypeClass)
		return MakeError(FString::Printf(TEXT("Unknown key type: %s"), *KeyType));

	// Check for duplicate
	for (const FBlackboardEntry& Existing : BB->Keys)
	{
		if (Existing.EntryName == FName(*KeyName))
			return MakeError(FString::Printf(TEXT("Key already exists: %s"), *KeyName));
	}

	// Add key
	FBlackboardEntry NewEntry;
	NewEntry.EntryName = FName(*KeyName);
	NewEntry.KeyType = NewObject<UBlackboardKeyType>(BB, *TypeClass);

	bool bInstanceSynced = false;
	Params->TryGetBoolField(TEXT("instance_synced"), bInstanceSynced);
	NewEntry.bInstanceSynced = bInstanceSynced ? 1 : 0;

	BB->Keys.Add(NewEntry);
	BB->Modify();
	UEditorAssetLibrary::SaveAsset(BlackboardPath, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("key_name"), KeyName);
	Data->SetStringField(TEXT("key_type"), KeyType);
	Data->SetNumberField(TEXT("key_count"), BB->Keys.Num());
	return MakeResponse(true, Data);
}

// Recursive serializer for BT node tree
static TSharedPtr<FJsonObject> SerializeBTNode(UBTCompositeNode* Node)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	if (!Node) return Obj;

	Obj->SetStringField(TEXT("name"), Node->GetNodeName());
	Obj->SetStringField(TEXT("class"), Node->GetClass()->GetName());

	// Note: Decorators/Services arrays are not publicly accessible on UBTCompositeNode in UE 5.7.
	// They can be read via reflection if needed in a future sprint.

	// Children
	TArray<TSharedPtr<FJsonValue>> Children;
	for (const FBTCompositeChild& Child : Node->Children)
	{
		if (Child.ChildComposite)
		{
			Children.Add(MakeShared<FJsonValueObject>(SerializeBTNode(Child.ChildComposite)));
		}
		else if (Child.ChildTask)
		{
			TSharedPtr<FJsonObject> TaskObj = MakeShared<FJsonObject>();
			TaskObj->SetStringField(TEXT("name"), Child.ChildTask->GetNodeName());
			TaskObj->SetStringField(TEXT("class"), Child.ChildTask->GetClass()->GetName());
			Children.Add(MakeShared<FJsonValueObject>(TaskObj));
		}
	}
	if (Children.Num() > 0) Obj->SetArrayField(TEXT("children"), Children);

	return Obj;
}

FString FMCPServer::HandleReadBehaviorTree(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString BTPath;
	if (!Params->TryGetStringField(TEXT("path"), BTPath))
		return MakeError(TEXT("path required"));

	UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
	if (!BT)
		return MakeError(FString::Printf(TEXT("Behavior tree not found: %s"), *BTPath));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), BTPath);
	Data->SetStringField(TEXT("blackboard"), BT->BlackboardAsset ? BT->BlackboardAsset->GetPathName() : TEXT("None"));

	if (BT->RootNode)
	{
		Data->SetObjectField(TEXT("root"), SerializeBTNode(BT->RootNode));
	}

	// If blackboard is set, list keys
	if (BT->BlackboardAsset)
	{
		TArray<TSharedPtr<FJsonValue>> Keys;
		for (const FBlackboardEntry& Entry : BT->BlackboardAsset->Keys)
		{
			TSharedPtr<FJsonObject> K = MakeShared<FJsonObject>();
			K->SetStringField(TEXT("name"), Entry.EntryName.ToString());
			K->SetStringField(TEXT("type"), Entry.KeyType ? Entry.KeyType->GetClass()->GetName() : TEXT("None"));
			Keys.Add(MakeShared<FJsonValueObject>(K));
		}
		Data->SetArrayField(TEXT("blackboard_keys"), Keys);
	}

	return MakeResponse(true, Data);
}
