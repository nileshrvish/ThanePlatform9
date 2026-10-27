#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraphPin.h"
#include "UObject/UnrealType.h"

class UBlueprint;
class UUserDefinedStruct;
class UEnum;
class UEdGraph;
class UEdGraphNode;

struct FSavedPinConnection
{
	FName PinName;
	EEdGraphPinDirection Direction;
	FGuid RemoteNodeGuid;
	FName RemotePinName;
};

UClass* ResolveParentClass(const FString& ParentClassPath);
void SerializePinType(const FEdGraphPinType& PinType, TSharedPtr<FJsonObject>& OutObj);
void SerializeProperty(const FProperty* Prop, TSharedPtr<FJsonObject>& OutObj);
bool DoesBlueprintReferenceStruct(UBlueprint* Blueprint, UUserDefinedStruct* OldStruct);
bool DoesBlueprintReferenceEnum(UBlueprint* Blueprint, UEnum* EnumToFind);
void SaveNodeConnections(UEdGraphNode* Node, TArray<FSavedPinConnection>& OutConnections);
void RestoreNodeConnections(
	UEdGraphNode* Node,
	const TArray<FSavedPinConnection>& SavedConnections,
	const TMap<FName, FName>& FieldNameMap,
	UEdGraph* Graph);
