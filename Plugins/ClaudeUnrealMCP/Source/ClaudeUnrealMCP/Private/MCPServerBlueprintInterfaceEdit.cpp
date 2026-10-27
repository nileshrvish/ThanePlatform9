#include "MCPServer.h"
#include "Engine/Blueprint.h"
#include "Animation/AnimBlueprint.h"
#include "WidgetBlueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Blueprint/BlueprintExtension.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "BlueprintEditorLibrary.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_SetFieldsInStruct.h"
#include "K2Node_BreakStruct.h"
#include "StructUtils/InstancedStruct.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_CastByteToEnum.h"
#include "K2Node_Select.h"
#include "K2Node_Message.h"
#include "EdGraphSchema_K2.h"
#include "StructUtils/UserDefinedStruct.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/TimelineTemplate.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "ObjectTools.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/RichCurve.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "Async/Async.h"
#include "UObject/SavePackage.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "Components/ActorComponent.h"
#include "Kismet2/ComponentEditorUtils.h"

FString FMCPServer::HandleDeleteInterfaceFunction(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("interface_path")) || !Params->HasField(TEXT("function_name")))
	{
		return MakeError(TEXT("Missing 'interface_path' or 'function_name' parameter"));
	}

	const FString InterfacePath = Params->GetStringField(TEXT("interface_path"));
	const FString FunctionName = Params->GetStringField(TEXT("function_name"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(InterfacePath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Interface not found: %s"), *InterfacePath));
	}

	if (Blueprint->BlueprintType != BPTYPE_Interface)
	{
		return MakeError(TEXT("Blueprint is not an interface"));
	}

	// Find the function graph
	UEdGraph* GraphToDelete = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			GraphToDelete = Graph;
			break;
		}
	}

	if (!GraphToDelete)
	{
		return MakeError(FString::Printf(TEXT("Function '%s' not found in interface"), *FunctionName));
	}

	// Remove the function graph
	Blueprint->FunctionGraphs.Remove(GraphToDelete);

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Function deleted successfully"));
	Data->SetStringField(TEXT("function_name"), FunctionName);
	Data->SetNumberField(TEXT("remaining_functions"), Blueprint->FunctionGraphs.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleModifyInterfaceFunctionParameter(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("interface_path")) ||
		!Params->HasField(TEXT("function_name")) || !Params->HasField(TEXT("parameter_name")) ||
		!Params->HasField(TEXT("new_type")))
	{
		return MakeError(TEXT("Missing required parameters: 'interface_path', 'function_name', 'parameter_name', 'new_type'"));
	}

	const FString InterfacePath = Params->GetStringField(TEXT("interface_path"));
	const FString FunctionName = Params->GetStringField(TEXT("function_name"));
	const FString ParameterName = Params->GetStringField(TEXT("parameter_name"));
	const FString NewType = Params->GetStringField(TEXT("new_type"));
	const bool bIsOutput = Params->HasField(TEXT("is_output")) ? Params->GetBoolField(TEXT("is_output")) : false;

	UBlueprint* Blueprint = LoadBlueprintFromPath(InterfacePath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Interface not found: %s"), *InterfacePath));
	}

	if (Blueprint->BlueprintType != BPTYPE_Interface)
	{
		return MakeError(TEXT("Blueprint is not an interface"));
	}

	// Find the function graph
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return MakeError(FString::Printf(TEXT("Function '%s' not found in interface"), *FunctionName));
	}

	// Parse the new type
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	FEdGraphPinType NewPinType;

	// Handle common types
	if (NewType == TEXT("int") || NewType == TEXT("int32"))
	{
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (NewType == TEXT("float") || NewType == TEXT("double"))
	{
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		NewPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (NewType == TEXT("bool") || NewType == TEXT("boolean"))
	{
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (NewType == TEXT("string") || NewType == TEXT("FString"))
	{
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (NewType == TEXT("FVector"))
	{
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		NewPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (NewType == TEXT("FRotator"))
	{
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		NewPinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (NewType == TEXT("FTransform"))
	{
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		NewPinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	}
	else if (NewType == TEXT("struct") && Params->HasField(TEXT("struct_type")))
	{
		// When new_type is "struct", use struct_type parameter to find the specific struct
		FString StructTypePath = Params->GetStringField(TEXT("struct_type"));
		FString StructNameToFind = StructTypePath;
		UScriptStruct* FoundStruct = nullptr;
		if (StructTypePath.StartsWith(TEXT("/Script/")))
		{
			FString Remainder = StructTypePath.RightChop(8);
			FString ModuleName;
			Remainder.Split(TEXT("."), &ModuleName, &StructNameToFind);
			// Use FindObject first to resolve exact C++ type (avoids BP name collisions)
			FoundStruct = FindObject<UScriptStruct>(nullptr, *StructTypePath);
		}
		if (!FoundStruct)
		{
			for (TObjectIterator<UScriptStruct> It; It; ++It)
			{
				if (It->GetName() == StructNameToFind)
				{
					FoundStruct = *It;
					break;
				}
			}
		}
		if (!FoundStruct)
		{
			return MakeError(FString::Printf(TEXT("Struct type not found: %s"), *StructTypePath));
		}
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		NewPinType.PinSubCategoryObject = FoundStruct;
	}
	else if (NewType == TEXT("enum") && Params->HasField(TEXT("enum_type")))
	{
		// When new_type is "enum", use enum_type parameter to find the specific enum
		FString EnumTypePath = Params->GetStringField(TEXT("enum_type"));
		FString EnumNameToFind = EnumTypePath;
		if (EnumTypePath.StartsWith(TEXT("/Script/")))
		{
			FString Remainder = EnumTypePath.RightChop(8);
			FString ModuleName;
			Remainder.Split(TEXT("."), &ModuleName, &EnumNameToFind);
		}
		UEnum* FoundEnum = nullptr;
		for (TObjectIterator<UEnum> It; It; ++It)
		{
			if (It->GetName() == EnumNameToFind)
			{
				FoundEnum = *It;
				break;
			}
		}
		if (!FoundEnum)
		{
			return MakeError(FString::Printf(TEXT("Enum type not found: %s"), *EnumTypePath));
		}
		NewPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		NewPinType.PinSubCategoryObject = FoundEnum;
	}
	else
	{
		// Try to load as object type or struct
		UObject* TypeObject = nullptr;
		FString TypeNameToFind = NewType;

		// Extract just the type name from /Script/ModuleName.TypeName format
		if (NewType.StartsWith(TEXT("/Script/")))
		{
			FString PathWithoutScript = NewType.RightChop(8);
			FString ModuleName;
			PathWithoutScript.Split(TEXT("."), &ModuleName, &TypeNameToFind);

			// For /Script/ paths, use FindObject first to resolve the exact C++ type
			// This avoids ambiguity when BP UserDefinedStructs share the same name
			TypeObject = FindObject<UScriptStruct>(nullptr, *NewType);
			if (!TypeObject)
			{
				TypeObject = FindObject<UClass>(nullptr, *NewType);
			}
		}

		// Fall back to TObjectIterator name search
		if (!TypeObject)
		{
			for (TObjectIterator<UScriptStruct> It; It; ++It)
			{
				UScriptStruct* Struct = *It;
				if (Struct->GetName() == TypeNameToFind)
				{
					TypeObject = Struct;
					break;
				}
			}
		}

		// If not found as struct, try classes
		if (!TypeObject)
		{
			for (TObjectIterator<UClass> It; It; ++It)
			{
				UClass* Class = *It;
				if (Class->GetName() == TypeNameToFind)
				{
					TypeObject = Class;
					break;
				}
			}
		}

		// Fallback to standard FindObject/LoadObject with full path
		if (!TypeObject)
		{
			TypeObject = FindObject<UObject>(nullptr, *NewType);
		}
		if (!TypeObject)
		{
			TypeObject = LoadObject<UObject>(nullptr, *NewType);
		}

		if (UClass* Class = Cast<UClass>(TypeObject))
		{
			NewPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			NewPinType.PinSubCategoryObject = Class;
		}
		else if (UScriptStruct* Struct = Cast<UScriptStruct>(TypeObject))
		{
			NewPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			NewPinType.PinSubCategoryObject = Struct;
		}
		else
		{
			return MakeError(FString::Printf(TEXT("Unknown type: %s. For C++ structs, ensure the struct has been used in the project (instantiated). C++ USTRUCT types may not be discoverable via reflection until they are actually used."), *NewType));
		}
	}

	// Find the entry or result node based on whether it's input or output
	UK2Node_EditablePinBase* TargetNode = nullptr;
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (bIsOutput)
		{
			UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(Node);
			if (ResultNode)
			{
				TargetNode = ResultNode;
				break;
			}
		}
		else
		{
			UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node);
			if (EntryNode)
			{
				TargetNode = EntryNode;
				break;
			}
		}
	}

	if (!TargetNode)
	{
		return MakeError(TEXT("Could not find entry/result node in function graph"));
	}

	// Find the existing pin and remove it
	bool bFoundPin = false;
	TArray<TSharedPtr<FUserPinInfo>> PinsToKeep;

	for (const TSharedPtr<FUserPinInfo>& PinInfo : TargetNode->UserDefinedPins)
	{
		if (PinInfo.IsValid() && PinInfo->PinName.ToString() == ParameterName)
		{
			bFoundPin = true;
			// Create a new pin info with the new type (and optionally new name)
			TSharedPtr<FUserPinInfo> NewPinInfo = MakeShared<FUserPinInfo>();
			FString NewName = Params->HasField(TEXT("new_name")) ? Params->GetStringField(TEXT("new_name")) : FString();
			NewPinInfo->PinName = NewName.IsEmpty() ? PinInfo->PinName : FName(*NewName);
			NewPinInfo->PinType = NewPinType;
			NewPinInfo->DesiredPinDirection = PinInfo->DesiredPinDirection;
			PinsToKeep.Add(NewPinInfo);
		}
		else
		{
			PinsToKeep.Add(PinInfo);
		}
	}

	if (!bFoundPin)
	{
		return MakeError(FString::Printf(TEXT("Parameter '%s' not found in function"), *ParameterName));
	}

	// Replace the user defined pins
	TargetNode->UserDefinedPins = PinsToKeep;

	// Reconstruct the node to reflect the changes
	TargetNode->ReconstructNode();

	// Mark the blueprint as modified and compile to update the generated class
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Interface function parameter modified successfully"));
	Data->SetStringField(TEXT("function_name"), FunctionName);
	Data->SetStringField(TEXT("parameter_name"), ParameterName);
	Data->SetStringField(TEXT("new_type"), NewType);

	return MakeResponse(true, Data);
}
