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

FString FMCPServer::HandleCreateBlueprintFunction(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) || !Params->HasField(TEXT("function_name")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' or 'function_name' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString FunctionName = Params->GetStringField(TEXT("function_name"));

	// Optional parameters
	const bool bIsPure = Params->HasField(TEXT("is_pure")) ? Params->GetBoolField(TEXT("is_pure")) : false;
	const bool bIsThreadSafe = Params->HasField(TEXT("is_thread_safe")) ? Params->GetBoolField(TEXT("is_thread_safe")) : false;
	const bool bIsConst = Params->HasField(TEXT("is_const")) ? Params->GetBoolField(TEXT("is_const")) : false;

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Check if function already exists
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == FunctionName)
		{
			return MakeError(FString::Printf(TEXT("Function '%s' already exists in blueprint"), *FunctionName));
		}
	}

	// Create new function graph using the blueprint editor utilities
	UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint,
		FName(*FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass()
	);

	if (!NewGraph)
	{
		return MakeError(TEXT("Failed to create function graph"));
	}

	// Initialize the graph schema
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	K2Schema->CreateDefaultNodesForGraph(*NewGraph);

	// Add to blueprint's function graphs array
	Blueprint->FunctionGraphs.Add(NewGraph);

	// Find the function entry node to set metadata
	UK2Node_FunctionEntry* EntryNode = nullptr;
	for (UEdGraphNode* Node : NewGraph->Nodes)
	{
		EntryNode = Cast<UK2Node_FunctionEntry>(Node);
		if (EntryNode)
		{
			break;
		}
	}

	if (EntryNode)
	{
		// Set function flags
		if (bIsPure)
		{
			EntryNode->AddExtraFlags(FUNC_BlueprintPure);
		}
		if (bIsThreadSafe)
		{
			EntryNode->MetaData.bThreadSafe = true;
		}
		if (bIsConst)
		{
			EntryNode->AddExtraFlags(FUNC_Const);
		}
	}

	// Mark blueprint as structurally modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Function created successfully"));
	Data->SetStringField(TEXT("function_name"), FunctionName);
	Data->SetBoolField(TEXT("is_pure"), bIsPure);
	Data->SetBoolField(TEXT("is_thread_safe"), bIsThreadSafe);
	Data->SetBoolField(TEXT("is_const"), bIsConst);
	Data->SetNumberField(TEXT("total_functions"), Blueprint->FunctionGraphs.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleAddFunctionInput(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("function_name")) || !Params->HasField(TEXT("parameter_name")) ||
		!Params->HasField(TEXT("parameter_type")))
	{
		return MakeError(TEXT("Missing required parameters: 'blueprint_path', 'function_name', 'parameter_name', 'parameter_type'"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString FunctionName = Params->GetStringField(TEXT("function_name"));
	const FString ParameterName = Params->GetStringField(TEXT("parameter_name"));
	const FString ParameterType = Params->GetStringField(TEXT("parameter_type"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
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
		return MakeError(FString::Printf(TEXT("Function '%s' not found in blueprint"), *FunctionName));
	}

	// Find the function entry node
	UK2Node_FunctionEntry* EntryNode = nullptr;
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		EntryNode = Cast<UK2Node_FunctionEntry>(Node);
		if (EntryNode)
		{
			break;
		}
	}

	if (!EntryNode)
	{
		return MakeError(TEXT("Function entry node not found"));
	}

	// Parse parameter type and create pin
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	FEdGraphPinType PinType;

	// Simple type parsing (can be expanded)
	if (ParameterType == TEXT("int") || ParameterType == TEXT("int32"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (ParameterType == TEXT("float") || ParameterType == TEXT("double"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (ParameterType == TEXT("bool") || ParameterType == TEXT("boolean"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (ParameterType == TEXT("string") || ParameterType == TEXT("FString"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (ParameterType == TEXT("FVector"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (ParameterType == TEXT("FRotator"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (ParameterType == TEXT("FTransform"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	}
	else
	{
		// Try to load as object type or struct
		UObject* TypeObject = FindObject<UObject>(nullptr, *ParameterType);
		if (!TypeObject)
		{
			TypeObject = LoadObject<UObject>(nullptr, *ParameterType);
		}

		if (UClass* Class = Cast<UClass>(TypeObject))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			PinType.PinSubCategoryObject = Class;
		}
		else if (UScriptStruct* Struct = Cast<UScriptStruct>(TypeObject))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			PinType.PinSubCategoryObject = Struct;
		}
		else
		{
			return MakeError(FString::Printf(TEXT("Unknown parameter type: %s"), *ParameterType));
		}
	}

	// Create the new user-defined pin
	EntryNode->CreateUserDefinedPin(FName(*ParameterName), PinType, EGPD_Output);

	// Mark blueprint as structurally modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Function input parameter added successfully"));
	Data->SetStringField(TEXT("function_name"), FunctionName);
	Data->SetStringField(TEXT("parameter_name"), ParameterName);
	Data->SetStringField(TEXT("parameter_type"), ParameterType);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleAddFunctionOutput(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("function_name")) || !Params->HasField(TEXT("parameter_name")) ||
		!Params->HasField(TEXT("parameter_type")))
	{
		return MakeError(TEXT("Missing required parameters: 'blueprint_path', 'function_name', 'parameter_name', 'parameter_type'"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString FunctionName = Params->GetStringField(TEXT("function_name"));
	const FString ParameterName = Params->GetStringField(TEXT("parameter_name"));
	const FString ParameterType = Params->GetStringField(TEXT("parameter_type"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
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
		return MakeError(FString::Printf(TEXT("Function '%s' not found in blueprint"), *FunctionName));
	}

	// Find the function result node
	UK2Node_FunctionResult* ResultNode = nullptr;
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		ResultNode = Cast<UK2Node_FunctionResult>(Node);
		if (ResultNode)
		{
			break;
		}
	}

	// If no result node exists, create one
	if (!ResultNode)
	{
		ResultNode = NewObject<UK2Node_FunctionResult>(FunctionGraph);
		ResultNode->CreateNewGuid();
		ResultNode->PostPlacedNewNode();
		ResultNode->AllocateDefaultPins();
		FunctionGraph->AddNode(ResultNode);
	}

	// Parse parameter type and create pin (same logic as input)
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	FEdGraphPinType PinType;

	if (ParameterType == TEXT("int") || ParameterType == TEXT("int32"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (ParameterType == TEXT("float") || ParameterType == TEXT("double"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (ParameterType == TEXT("bool") || ParameterType == TEXT("boolean"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (ParameterType == TEXT("string") || ParameterType == TEXT("FString"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (ParameterType == TEXT("FVector"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
	}
	else if (ParameterType == TEXT("FRotator"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
	}
	else if (ParameterType == TEXT("FTransform"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
	}
	else
	{
		UObject* TypeObject = FindObject<UObject>(nullptr, *ParameterType);
		if (!TypeObject)
		{
			TypeObject = LoadObject<UObject>(nullptr, *ParameterType);
		}

		if (UClass* Class = Cast<UClass>(TypeObject))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			PinType.PinSubCategoryObject = Class;
		}
		else if (UScriptStruct* Struct = Cast<UScriptStruct>(TypeObject))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			PinType.PinSubCategoryObject = Struct;
		}
		else
		{
			return MakeError(FString::Printf(TEXT("Unknown parameter type: %s"), *ParameterType));
		}
	}

	// Create the new user-defined pin for output
	ResultNode->CreateUserDefinedPin(FName(*ParameterName), PinType, EGPD_Input);

	// Mark blueprint as structurally modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Function output parameter added successfully"));
	Data->SetStringField(TEXT("function_name"), FunctionName);
	Data->SetStringField(TEXT("parameter_name"), ParameterName);
	Data->SetStringField(TEXT("parameter_type"), ParameterType);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleRenameBlueprintFunction(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) ||
		!Params->HasField(TEXT("old_function_name")) || !Params->HasField(TEXT("new_function_name")))
	{
		return MakeError(TEXT("Missing required parameters: 'blueprint_path', 'old_function_name', 'new_function_name'"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString OldFunctionName = Params->GetStringField(TEXT("old_function_name"));
	const FString NewFunctionName = Params->GetStringField(TEXT("new_function_name"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Find the function graph to rename
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == OldFunctionName)
		{
			FunctionGraph = Graph;
			break;
		}
	}

	if (!FunctionGraph)
	{
		return MakeError(FString::Printf(TEXT("Function '%s' not found in blueprint"), *OldFunctionName));
	}

	// Check if new name already exists
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && Graph != FunctionGraph && Graph->GetName() == NewFunctionName)
		{
			return MakeError(FString::Printf(TEXT("Function '%s' already exists in blueprint"), *NewFunctionName));
		}
	}

	// Rename the graph
	FBlueprintEditorUtils::RenameGraph(FunctionGraph, NewFunctionName);

	// Mark blueprint as structurally modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Function renamed successfully"));
	Data->SetStringField(TEXT("old_name"), OldFunctionName);
	Data->SetStringField(TEXT("new_name"), NewFunctionName);

	return MakeResponse(true, Data);
}

/**
 * HandleReadActorProperties - Read actual property values from a level actor instance
 *
 * RECOMMENDED METHOD (2026-02-03):
 * This is the CORRECT way to read actual working values from a Blueprint actor in a level.
 *
 * WHY USE THIS INSTEAD OF read_class_defaults:
 * - Reads property overrides set in the Details panel (stored in level .umap file)
 * - Returns ACTUAL gameplay-tested values, not class defaults
 * - Gets the real working behavior from a reference project
 * - Essential for accurate Blueprint-to-C++ conversion
 *
 * WORKFLOW FOR COPYING REFERENCE DATA:
 * When you need to copy correct values from a working project (e.g., GameAnimationSample):
 * 1. Copy the level file (.umap) from the working project to your project
 * 2. Open the level in Unreal Editor
 * 3. Use this command to read the actor instance
 * 4. Extract the property values from the response
 * 5. Use those values in your C++ implementation
 *
 * EXAMPLE - LevelVisuals Style Values:
 * - Blueprint CDO (read_class_defaults): FogColor = (0.79, 0.75, 1.0) ❌ Wrong
 * - Level Instance (this function): FogColor = (0.261, 0.261, 0.302) ✅ Correct
 *
 * The level instance values match the actual gameplay behavior seen in the reference project.
 *
 * USE CASES:
 * - Preserving actor configuration before blueprint reparenting
 * - Getting reference values from a working project for C++ conversion
 * - Debugging property override issues
 * - Comparing class defaults vs instance values
 */
