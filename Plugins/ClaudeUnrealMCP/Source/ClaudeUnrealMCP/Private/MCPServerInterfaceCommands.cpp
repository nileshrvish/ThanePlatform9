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

FString FMCPServer::HandleRemoveImplementedInterface(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString InterfaceName = Params->GetStringField(TEXT("interface_name"));

	if (BlueprintPath.IsEmpty() || InterfaceName.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path or interface_name"));
	}

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Find and remove the implemented interface
	int32 RemovedCount = 0;
	for (int32 i = Blueprint->ImplementedInterfaces.Num() - 1; i >= 0; i--)
	{
		FBPInterfaceDescription& Interface = Blueprint->ImplementedInterfaces[i];
		if (Interface.Interface)
		{
			FString ClassName = Interface.Interface->GetName();
			if (ClassName.Contains(InterfaceName))
			{
				// Remove any graphs associated with this interface
				for (UEdGraph* Graph : Interface.Graphs)
				{
					if (Graph)
					{
						FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph);
					}
				}
				Blueprint->ImplementedInterfaces.RemoveAt(i);
				RemovedCount++;
			}
		}
	}

	if (RemovedCount == 0)
	{
		return MakeError(FString::Printf(TEXT("Interface '%s' not found in blueprint"), *InterfaceName));
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Blueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Removed %d interface(s) matching '%s'"), RemovedCount, *InterfaceName));
	Data->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	Data->SetNumberField(TEXT("interfaces_removed"), RemovedCount);
	Data->SetNumberField(TEXT("remaining_interfaces"), Blueprint->ImplementedInterfaces.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleAddImplementedInterface(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	FString InterfacePath = Params->GetStringField(TEXT("interface_path"));

	if (BlueprintPath.IsEmpty() || InterfacePath.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters: blueprint_path or interface_path"));
	}

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// Load the interface class - support both BP interface assets and C++ interfaces
	UClass* InterfaceClass = nullptr;

	// Try loading as a Blueprint interface asset first
	UBlueprint* InterfaceBP = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *InterfacePath));
	if (InterfaceBP && InterfaceBP->GeneratedClass)
	{
		InterfaceClass = InterfaceBP->GeneratedClass;
	}
	else
	{
		// Try as a C++ class path (e.g., /Script/ModuleName.ClassName)
		InterfaceClass = LoadClass<UInterface>(nullptr, *InterfacePath);
	}

	if (!InterfaceClass)
	{
		return MakeError(FString::Printf(TEXT("Interface not found: %s"), *InterfacePath));
	}

	// Check if already implemented
	for (const FBPInterfaceDescription& Existing : Blueprint->ImplementedInterfaces)
	{
		if (Existing.Interface == InterfaceClass)
		{
			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetStringField(TEXT("message"), TEXT("Interface already implemented"));
			Data->SetStringField(TEXT("blueprint_path"), BlueprintPath);
			Data->SetStringField(TEXT("interface_class"), InterfaceClass->GetName());
			return MakeResponse(true, Data);
		}
	}

	bool bSkipGraphs = Params->HasField(TEXT("skip_graphs")) && Params->GetBoolField(TEXT("skip_graphs"));

	if (bSkipGraphs)
	{
		// Manually add the interface entry without creating override function graphs.
		// This is needed when the C++ parent class already provides _Implementation functions
		// and creating BP override graphs would cause "Cannot order parameters" conflicts.
		FBPInterfaceDescription NewInterface;
		NewInterface.Interface = InterfaceClass;
		Blueprint->ImplementedInterfaces.Add(NewInterface);
	}
	else
	{
		// Use the editor utility to properly add the interface (creates graphs, etc.)
		FBlueprintEditorUtils::ImplementNewInterface(Blueprint, InterfaceClass->GetClassPathName());
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Blueprint->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Added interface '%s' to blueprint"), *InterfaceClass->GetName()));
	Data->SetStringField(TEXT("blueprint_path"), BlueprintPath);
	Data->SetStringField(TEXT("interface_class"), InterfaceClass->GetName());
	Data->SetNumberField(TEXT("total_interfaces"), Blueprint->ImplementedInterfaces.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleMigrateInterfaceReferences(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing parameters"));
	}

	FString OldInterfacePath = Params->GetStringField(TEXT("old_interface_path"));
	FString NewInterfacePath = Params->GetStringField(TEXT("new_interface_path"));

	if (OldInterfacePath.IsEmpty() || NewInterfacePath.IsEmpty())
	{
		return MakeError(TEXT("Missing required parameters: old_interface_path, new_interface_path"));
	}

	// Load old interface class (BP interface)
	UClass* OldInterfaceClass = nullptr;
	UBlueprint* OldInterfaceBP = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *OldInterfacePath));
	if (OldInterfaceBP && OldInterfaceBP->GeneratedClass)
	{
		OldInterfaceClass = OldInterfaceBP->GeneratedClass;
	}
	if (!OldInterfaceClass)
	{
		return MakeError(FString::Printf(TEXT("Old interface not found: %s"), *OldInterfacePath));
	}

	// Load new interface class (C++ interface)
	UClass* NewInterfaceClass = nullptr;
	if (NewInterfacePath.StartsWith(TEXT("/Script/")))
	{
		NewInterfaceClass = FindObject<UClass>(nullptr, *NewInterfacePath);
		if (!NewInterfaceClass)
		{
			NewInterfaceClass = LoadClass<UInterface>(nullptr, *NewInterfacePath);
		}
	}
	if (!NewInterfaceClass)
	{
		// Try by name
		FString ClassName = FPackageName::ObjectPathToObjectName(NewInterfacePath);
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->IsChildOf(UInterface::StaticClass()) && It->GetName() == ClassName)
			{
				NewInterfaceClass = *It;
				break;
			}
		}
	}
	if (!NewInterfaceClass)
	{
		return MakeError(FString::Printf(TEXT("New interface not found: %s"), *NewInterfacePath));
	}

	// Optional: limit to specific blueprints
	TArray<FString> TargetBlueprintPaths;
	if (Params->HasField(TEXT("blueprint_paths")))
	{
		const TArray<TSharedPtr<FJsonValue>>& PathsArray = Params->GetArrayField(TEXT("blueprint_paths"));
		for (const auto& Val : PathsArray)
		{
			TargetBlueprintPaths.Add(Val->AsString());
		}
	}

	int32 TotalNodesFixed = 0;
	int32 TotalBPsAffected = 0;
	TArray<TSharedPtr<FJsonValue>> AffectedBPs;

	// Helper lambda to process a single blueprint
	auto ProcessBlueprint = [&](UBlueprint* Blueprint) -> int32
	{
		int32 NodesFixed = 0;

		// Collect all graphs: event graphs, function graphs, interface graphs
		TArray<UEdGraph*> AllGraphs;
		AllGraphs.Append(Blueprint->UbergraphPages);
		AllGraphs.Append(Blueprint->FunctionGraphs);
		for (const FBPInterfaceDescription& InterfaceDesc : Blueprint->ImplementedInterfaces)
		{
			AllGraphs.Append(InterfaceDesc.Graphs);
		}

		for (UEdGraph* Graph : AllGraphs)
		{
			if (!Graph) continue;

			// Also include sub-graphs (collapsed graphs, etc.)
			TArray<UEdGraph*> GraphsToCheck;
			GraphsToCheck.Add(Graph);
			Graph->GetAllChildrenGraphs(GraphsToCheck);

			for (UEdGraph* SubGraph : GraphsToCheck)
			{
				for (UEdGraphNode* Node : SubGraph->Nodes)
				{
					UK2Node_Message* MessageNode = Cast<UK2Node_Message>(Node);
					if (!MessageNode) continue;

					// Check if this message node references the old interface
					UClass* MemberParentClass = MessageNode->FunctionReference.GetMemberParentClass(
						MessageNode->GetBlueprintClassFromNode());

					if (MemberParentClass == OldInterfaceClass)
					{
						// Get the function name before changing
						FName FuncName = MessageNode->FunctionReference.GetMemberName();

						// Verify the new interface has this function
						UFunction* NewFunc = NewInterfaceClass->FindFunctionByName(FuncName);
						if (!NewFunc)
						{
							UE_LOG(LogTemp, Warning, TEXT("MigrateInterfaceReferences: Function '%s' not found on new interface '%s', skipping node in %s"),
								*FuncName.ToString(), *NewInterfaceClass->GetName(), *Blueprint->GetName());
							continue;
						}

						// Change the interface reference
						MessageNode->FunctionReference.SetExternalMember(FuncName, NewInterfaceClass);

						// Reconstruct the node to update pins
						MessageNode->ReconstructNode();

						NodesFixed++;
					}
				}
			}
		}

		return NodesFixed;
	};

	if (TargetBlueprintPaths.Num() > 0)
	{
		// Process specific blueprints
		for (const FString& BPPath : TargetBlueprintPaths)
		{
			UBlueprint* BP = LoadBlueprintFromPath(BPPath);
			if (!BP) continue;

			int32 Fixed = ProcessBlueprint(BP);
			if (Fixed > 0)
			{
				TotalNodesFixed += Fixed;
				TotalBPsAffected++;
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
				BP->MarkPackageDirty();

				TSharedPtr<FJsonObject> BPInfo = MakeShared<FJsonObject>();
				BPInfo->SetStringField(TEXT("name"), BP->GetName());
				BPInfo->SetNumberField(TEXT("nodes_fixed"), Fixed);
				AffectedBPs.Add(MakeShared<FJsonValueObject>(BPInfo));
			}
		}
	}
	else
	{
		// Process ALL loaded blueprints
		for (TObjectIterator<UBlueprint> It; It; ++It)
		{
			UBlueprint* BP = *It;
			if (!BP || !IsValid(BP)) continue;
			// Skip engine/plugin content
			FString PackageName = BP->GetOutermost()->GetName();
			if (!PackageName.StartsWith(TEXT("/Game/"))) continue;

			int32 Fixed = ProcessBlueprint(BP);
			if (Fixed > 0)
			{
				TotalNodesFixed += Fixed;
				TotalBPsAffected++;
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
				BP->MarkPackageDirty();

				TSharedPtr<FJsonObject> BPInfo = MakeShared<FJsonObject>();
				BPInfo->SetStringField(TEXT("name"), BP->GetName());
				BPInfo->SetStringField(TEXT("path"), PackageName);
				BPInfo->SetNumberField(TEXT("nodes_fixed"), Fixed);
				AffectedBPs.Add(MakeShared<FJsonValueObject>(BPInfo));
			}
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Migrated %d K2Node_Message nodes across %d blueprints"), TotalNodesFixed, TotalBPsAffected));
	Data->SetStringField(TEXT("old_interface"), OldInterfaceClass->GetName());
	Data->SetStringField(TEXT("new_interface"), NewInterfaceClass->GetName());
	Data->SetNumberField(TEXT("total_nodes_fixed"), TotalNodesFixed);
	Data->SetNumberField(TEXT("total_blueprints_affected"), TotalBPsAffected);
	Data->SetArrayField(TEXT("affected_blueprints"), AffectedBPs);

	return MakeResponse(true, Data);
}
