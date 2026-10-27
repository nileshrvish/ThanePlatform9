#include "MCPServer.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "AssetExportTask.h"
#include "Exporters/Exporter.h"
#include "Misc/Paths.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphUtilities.h"

// Serialize a UObject's properties into a JSON object for inspection.
// Recursively inlines Instanced sub-objects up to MaxDepth.
static TSharedPtr<FJsonObject> SerializeObjectToJson(UObject* Object, int32 Depth, int32 MaxDepth)
{
	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	if (!Object || Depth > MaxDepth)
	{
		return Out;
	}

	Out->SetStringField(TEXT("__class"), Object->GetClass()->GetName());
	Out->SetStringField(TEXT("__path"), Object->GetPathName());

	for (TFieldIterator<FProperty> PropIt(Object->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		const FString PropName = Prop->GetName();

		// Inlined sub-object (Instanced)
		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
		{
			UObject* SubObj = ObjProp->GetObjectPropertyValue_InContainer(Object);
			if (SubObj)
			{
				if (Prop->HasAnyPropertyFlags(CPF_InstancedReference) && Depth < MaxDepth)
				{
					Out->SetObjectField(PropName, SerializeObjectToJson(SubObj, Depth + 1, MaxDepth));
				}
				else
				{
					Out->SetStringField(PropName, SubObj->GetPathName());
				}
			}
			else
			{
				Out->SetField(PropName, MakeShared<FJsonValueNull>());
			}
			continue;
		}

		// Arrays
		if (FArrayProperty* ArrProp = CastField<FArrayProperty>(Prop))
		{
			TArray<TSharedPtr<FJsonValue>> Items;
			FScriptArrayHelper Helper(ArrProp, ArrProp->ContainerPtrToValuePtr<void>(Object));
			const int32 Num = Helper.Num();
			for (int32 Idx = 0; Idx < Num; ++Idx)
			{
				uint8* ElemPtr = Helper.GetRawPtr(Idx);
				if (FObjectProperty* ElemObj = CastField<FObjectProperty>(ArrProp->Inner))
				{
					UObject* SubObj = ElemObj->GetObjectPropertyValue(ElemPtr);
					if (SubObj && ArrProp->Inner->HasAnyPropertyFlags(CPF_InstancedReference) && Depth < MaxDepth)
					{
						Items.Add(MakeShared<FJsonValueObject>(SerializeObjectToJson(SubObj, Depth + 1, MaxDepth)));
					}
					else
					{
						Items.Add(MakeShared<FJsonValueString>(SubObj ? SubObj->GetPathName() : TEXT("None")));
					}
				}
				else
				{
					FString ElemStr;
					ArrProp->Inner->ExportText_Direct(ElemStr, ElemPtr, ElemPtr, nullptr, PPF_None);
					Items.Add(MakeShared<FJsonValueString>(ElemStr));
				}
			}
			Out->SetArrayField(PropName, Items);
			continue;
		}

		// Simple properties (numbers, strings, enums, structs) — export as text
		FString ValStr;
		Prop->ExportText_InContainer(0, ValStr, Object, Object, Object, PPF_None);
		if (!ValStr.IsEmpty())
		{
			Out->SetStringField(PropName, ValStr);
		}
	}
	return Out;
}

FString FMCPServer::HandleRunPython(const TSharedPtr<FJsonObject>& Params)
{
	// Python plugin caused editor crashes due to PostLoad assertion when enabled in uproject.
	// Provide targeted asset manipulation commands via this handler instead (repurposed).
	// Operation parameter chooses what to do:
	//   op=duplicate_asset: params.source_path, params.dest_path
	//   op=does_asset_exist: params.path
	//   op=save_asset: params.path
	//   op=export_fbx: params.path, params.out_file (absolute .fbx path)

	if (!Params.IsValid())
	{
		return MakeError(TEXT("Missing params"));
	}

	FString Op;
	if (!Params->TryGetStringField(TEXT("op"), Op) || Op.IsEmpty())
	{
		return MakeError(TEXT("Missing 'op' parameter. Supported: duplicate_asset, does_asset_exist, save_asset, export_fbx"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

	if (Op == TEXT("duplicate_asset"))
	{
		FString SourcePath, DestPath;
		if (!Params->TryGetStringField(TEXT("source_path"), SourcePath) ||
			!Params->TryGetStringField(TEXT("dest_path"), DestPath))
		{
			return MakeError(TEXT("duplicate_asset requires source_path and dest_path"));
		}

		if (!UEditorAssetLibrary::DoesAssetExist(SourcePath))
		{
			return MakeError(FString::Printf(TEXT("Source asset not found: %s"), *SourcePath));
		}

		if (UEditorAssetLibrary::DoesAssetExist(DestPath))
		{
			Data->SetBoolField(TEXT("already_exists"), true);
			Data->SetStringField(TEXT("dest_path"), DestPath);
			return MakeResponse(true, Data);
		}

		UObject* NewAsset = UEditorAssetLibrary::DuplicateAsset(SourcePath, DestPath);
		if (!NewAsset)
		{
			return MakeError(FString::Printf(TEXT("Failed to duplicate %s to %s"), *SourcePath, *DestPath));
		}

		UEditorAssetLibrary::SaveAsset(DestPath, false);
		Data->SetStringField(TEXT("dest_path"), DestPath);
		Data->SetBoolField(TEXT("already_exists"), false);
		return MakeResponse(true, Data);
	}
	else if (Op == TEXT("does_asset_exist"))
	{
		FString Path;
		if (!Params->TryGetStringField(TEXT("path"), Path))
		{
			return MakeError(TEXT("does_asset_exist requires path"));
		}
		Data->SetBoolField(TEXT("exists"), UEditorAssetLibrary::DoesAssetExist(Path));
		return MakeResponse(true, Data);
	}
	else if (Op == TEXT("save_asset"))
	{
		FString Path;
		if (!Params->TryGetStringField(TEXT("path"), Path))
		{
			return MakeError(TEXT("save_asset requires path"));
		}
		const bool bOK = UEditorAssetLibrary::SaveAsset(Path, false);
		Data->SetBoolField(TEXT("saved"), bOK);
		return MakeResponse(bOK, Data);
	}
	else if (Op == TEXT("inspect_asset"))
	{
		FString Path;
		if (!Params->TryGetStringField(TEXT("path"), Path))
		{
			return MakeError(TEXT("inspect_asset requires path"));
		}
		int32 MaxDepth = 4;
		Params->TryGetNumberField(TEXT("max_depth"), MaxDepth);

		UObject* Asset = UEditorAssetLibrary::LoadAsset(Path);
		if (!Asset)
		{
			return MakeError(FString::Printf(TEXT("Could not load asset: %s"), *Path));
		}
		Data->SetObjectField(TEXT("asset"), SerializeObjectToJson(Asset, 0, MaxDepth));
		return MakeResponse(true, Data);
	}
	else if (Op == TEXT("set_property"))
	{
		// Set a top-level property on an asset object by name.
		// params: path, property, value (string, parsed as FProperty text import)
		FString Path, PropName, ValueText;
		if (!Params->TryGetStringField(TEXT("path"), Path) ||
			!Params->TryGetStringField(TEXT("property"), PropName) ||
			!Params->TryGetStringField(TEXT("value"), ValueText))
		{
			return MakeError(TEXT("set_property requires path, property, value"));
		}
		UObject* Asset = UEditorAssetLibrary::LoadAsset(Path);
		if (!Asset)
		{
			return MakeError(FString::Printf(TEXT("Could not load asset: %s"), *Path));
		}
		FProperty* Prop = FindFProperty<FProperty>(Asset->GetClass(), *PropName);
		if (!Prop)
		{
			return MakeError(FString::Printf(TEXT("Property not found: %s"), *PropName));
		}
		void* ValPtr = Prop->ContainerPtrToValuePtr<void>(Asset);
		const TCHAR* Result = Prop->ImportText_Direct(*ValueText, ValPtr, Asset, PPF_None);
		if (!Result)
		{
			return MakeError(FString::Printf(TEXT("Failed to import value '%s' for property %s"), *ValueText, *PropName));
		}
		Asset->Modify();
		UEditorAssetLibrary::SaveAsset(Path, false);
		Data->SetBoolField(TEXT("set"), true);
		return MakeResponse(true, Data);
	}

	else if (Op == TEXT("open_asset"))
	{
		FString Path;
		if (!Params->TryGetStringField(TEXT("path"), Path))
			return MakeError(TEXT("path required"));
		UObject* Asset = UEditorAssetLibrary::LoadAsset(Path);
		if (!Asset)
			return MakeError(FString::Printf(TEXT("Asset not found: %s"), *Path));
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset);
		Data->SetStringField(TEXT("opened"), Path);
		return MakeResponse(true, Data);
	}
	else if (Op == TEXT("set_subobject_property"))
	{
		FString Path, SubObjPath, PropName, ValueText;
		if (!Params->TryGetStringField(TEXT("path"), Path) ||
			!Params->TryGetStringField(TEXT("subobject"), SubObjPath) ||
			!Params->TryGetStringField(TEXT("property"), PropName) ||
			!Params->TryGetStringField(TEXT("value"), ValueText))
		{
			return MakeError(TEXT("set_subobject_property requires path, subobject, property, value"));
		}
		UObject* Asset = UEditorAssetLibrary::LoadAsset(Path);
		if (!Asset)
			return MakeError(FString::Printf(TEXT("Asset not found: %s"), *Path));

		UObject* Current = Asset;
		TArray<FString> PathParts;
		SubObjPath.ParseIntoArray(PathParts, TEXT("."));
		for (const FString& Part : PathParts)
		{
			FObjectProperty* ObjProp = FindFProperty<FObjectProperty>(Current->GetClass(), *Part);
			if (!ObjProp)
				return MakeError(FString::Printf(TEXT("Property '%s' not found on %s"), *Part, *Current->GetClass()->GetName()));
			UObject* Next = ObjProp->GetObjectPropertyValue_InContainer(Current);
			if (!Next)
				return MakeError(FString::Printf(TEXT("Sub-object '%s' is null"), *Part));
			Current = Next;
		}

		FProperty* Prop = FindFProperty<FProperty>(Current->GetClass(), *PropName);
		if (!Prop)
			return MakeError(FString::Printf(TEXT("Property '%s' not found on %s"), *PropName, *Current->GetClass()->GetName()));

		void* ValPtr = Prop->ContainerPtrToValuePtr<void>(Current);
		const TCHAR* Result = Prop->ImportText_Direct(*ValueText, ValPtr, Current, PPF_None);
		if (!Result)
			return MakeError(FString::Printf(TEXT("Failed to import value for %s"), *PropName));

		Asset->Modify();
		Current->Modify();
		UEditorAssetLibrary::SaveAsset(Path, false);
		Data->SetBoolField(TEXT("set"), true);
		return MakeResponse(true, Data);
	}

	else if (Op == TEXT("export_fbx"))
	{
		// Export an AnimSequence/SkeletalMesh/etc. asset to an FBX file on disk.
		// params: path (asset path, e.g. /Game/Characters/.../M_Neutral_Idle_turn_left),
		//         out_file (absolute filesystem path ending in .fbx)
		FString Path, OutFile;
		if (!Params->TryGetStringField(TEXT("path"), Path) ||
			!Params->TryGetStringField(TEXT("out_file"), OutFile))
		{
			return MakeError(TEXT("export_fbx requires path and out_file"));
		}

		UObject* Asset = UEditorAssetLibrary::LoadAsset(Path);
		if (!Asset)
		{
			return MakeError(FString::Printf(TEXT("Asset not found: %s"), *Path));
		}

		UExporter* Exporter = UExporter::FindExporter(Asset, *FPaths::GetExtension(OutFile));
		if (!Exporter)
		{
			return MakeError(FString::Printf(TEXT("No exporter found for asset class %s with extension %s"),
				*Asset->GetClass()->GetName(), *FPaths::GetExtension(OutFile)));
		}

		UAssetExportTask* ExportTask = NewObject<UAssetExportTask>();
		ExportTask->Object = Asset;
		ExportTask->Exporter = Exporter;
		ExportTask->Filename = OutFile;
		ExportTask->bSelected = false;
		ExportTask->bReplaceIdentical = true;
		ExportTask->bPrompt = false;
		ExportTask->bUseFileArchive = false;
		ExportTask->bWriteEmptyFiles = false;
		ExportTask->bAutomated = true;

		const bool bSuccess = UExporter::RunAssetExportTask(ExportTask);
		Data->SetBoolField(TEXT("exported"), bSuccess);
		Data->SetStringField(TEXT("out_file"), OutFile);
		if (!bSuccess)
		{
			return MakeError(FString::Printf(TEXT("Export failed for %s -> %s"), *Path, *OutFile));
		}
		return MakeResponse(true, Data);
	}

	else if (Op == TEXT("inspect_graph_node") || Op == TEXT("set_graph_node_property"))
	{
		// Reads (or sets) properties on a graph node by GUID that live on the
		// node object itself, not exposed as a pin - e.g. AnimGraphNode_TwoBoneIK's
		// EffectorTarget (bone/socket the IK reaches for), which read_function_graphs
		// doesn't surface since it only walks Pins. Same node-lookup pattern as
		// set_pin_default/connect_nodes (Blueprint->GetAllGraphs + GUID match) -
		// works for both nested AnimGraph sub-graphs (GetAllGraphs recurses into
		// state machines/anim layers already) and plain function graphs.
		FString BPPath, GraphName, NodeGuid;
		if (!Params->TryGetStringField(TEXT("blueprint_path"), BPPath) ||
			!Params->TryGetStringField(TEXT("graph_name"), GraphName) ||
			!Params->TryGetStringField(TEXT("node_guid"), NodeGuid))
		{
			return MakeError(TEXT("blueprint_path, graph_name, node_guid required"));
		}

		UBlueprint* Blueprint = LoadBlueprintFromPath(BPPath);
		if (!Blueprint)
		{
			return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));
		}

		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);

		UEdGraphNode* FoundNode = nullptr;
		for (UEdGraph* Graph : AllGraphs)
		{
			if (Graph->GetName() != GraphName)
			{
				continue;
			}
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node->NodeGuid.ToString() == NodeGuid ||
					Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces) == NodeGuid ||
					Node->NodeGuid.ToString(EGuidFormats::Digits) == NodeGuid)
				{
					FoundNode = Node;
					break;
				}
			}
			if (FoundNode)
			{
				break;
			}
		}

		if (!FoundNode)
		{
			return MakeError(FString::Printf(TEXT("Node with GUID '%s' not found in graph '%s'"), *NodeGuid, *GraphName));
		}

		if (Op == TEXT("inspect_graph_node"))
		{
			int32 MaxDepth = 2;
			Params->TryGetNumberField(TEXT("max_depth"), MaxDepth);
			Data->SetObjectField(TEXT("node"), SerializeObjectToJson(FoundNode, 0, MaxDepth));
			return MakeResponse(true, Data);
		}
		else
		{
			FString PropName, ValueText;
			if (!Params->TryGetStringField(TEXT("property"), PropName) ||
				!Params->TryGetStringField(TEXT("value"), ValueText))
			{
				return MakeError(TEXT("set_graph_node_property requires property, value"));
			}

			FProperty* Prop = FindFProperty<FProperty>(FoundNode->GetClass(), *PropName);
			if (!Prop)
			{
				return MakeError(FString::Printf(TEXT("Property '%s' not found on node class %s"), *PropName, *FoundNode->GetClass()->GetName()));
			}

			void* ValPtr = Prop->ContainerPtrToValuePtr<void>(FoundNode);
			const TCHAR* Result = Prop->ImportText_Direct(*ValueText, ValPtr, FoundNode, PPF_None);
			if (!Result)
			{
				return MakeError(FString::Printf(TEXT("Failed to import value '%s' for property %s"), *ValueText, *PropName));
			}

			FoundNode->Modify();
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			UEditorAssetLibrary::SaveAsset(BPPath, false);
			Data->SetBoolField(TEXT("set"), true);
			return MakeResponse(true, Data);
		}
	}

	else if (Op == TEXT("duplicate_node"))
	{
		// Clone an existing graph node (any K2Node type) via copy/paste text serialization -
		// FEdGraphUtilities::ExportNodesToText/ImportNodesFromText is the same path the editor's
		// own Ctrl+C/Ctrl+V uses, so pin defaults, node-specific properties, and comments all
		// carry over correctly (unlike hand-rolling NewObject+AllocateDefaultPins, which loses
		// anything not set by AllocateDefaultPins). Links to nodes outside the copied set are
		// dropped automatically, so the clone starts fully disconnected - wire it up afterward
		// with connect_nodes and re-point its literal pin defaults with set_pin_default.
		// params: blueprint_path, graph_name, node_guid (source node to clone)
		// optional: dx, dy (position offset for the new node relative to the source; default 0, 200)
		FString BPPath, GraphName, NodeGuidStr;
		if (!Params->TryGetStringField(TEXT("blueprint_path"), BPPath) ||
			!Params->TryGetStringField(TEXT("graph_name"), GraphName) ||
			!Params->TryGetStringField(TEXT("node_guid"), NodeGuidStr))
		{
			return MakeError(TEXT("duplicate_node requires blueprint_path, graph_name, node_guid"));
		}

		UBlueprint* Blueprint = LoadBlueprintFromPath(BPPath);
		if (!Blueprint)
		{
			return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BPPath));
		}

		TArray<UEdGraph*> AllGraphs;
		Blueprint->GetAllGraphs(AllGraphs);

		UEdGraph* TargetGraph = nullptr;
		UEdGraphNode* SourceNode = nullptr;
		for (UEdGraph* Graph : AllGraphs)
		{
			if (!Graph || Graph->GetName() != GraphName) continue;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node && (Node->NodeGuid.ToString() == NodeGuidStr ||
					Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces) == NodeGuidStr ||
					Node->NodeGuid.ToString(EGuidFormats::Digits) == NodeGuidStr))
				{
					SourceNode = Node;
					TargetGraph = Graph;
					break;
				}
			}
			if (SourceNode) break;
		}

		if (!SourceNode || !TargetGraph)
		{
			return MakeError(FString::Printf(TEXT("Node with GUID '%s' not found in graph '%s'"), *NodeGuidStr, *GraphName));
		}

		FString ExportedText;
		FEdGraphUtilities::ExportNodesToText(TSet<UObject*>{ SourceNode }, ExportedText);
		if (ExportedText.IsEmpty())
		{
			return MakeError(TEXT("Failed to export source node for duplication"));
		}

		TSet<UEdGraphNode*> ImportedNodes;
		FEdGraphUtilities::ImportNodesFromText(TargetGraph, ExportedText, ImportedNodes);

		if (ImportedNodes.Num() != 1)
		{
			return MakeError(FString::Printf(TEXT("Expected 1 imported node, got %d"), ImportedNodes.Num()));
		}

		UEdGraphNode* NewNode = ImportedNodes.Array()[0];
		NewNode->CreateNewGuid();

		double DX = 0.0, DY = 200.0;
		Params->TryGetNumberField(TEXT("dx"), DX);
		Params->TryGetNumberField(TEXT("dy"), DY);
		NewNode->NodePosX += static_cast<int32>(DX);
		NewNode->NodePosY += static_cast<int32>(DY);

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

		Data->SetStringField(TEXT("new_node_id"), NewNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
		Data->SetNumberField(TEXT("pos_x"), NewNode->NodePosX);
		Data->SetNumberField(TEXT("pos_y"), NewNode->NodePosY);
		return MakeResponse(true, Data);
	}

	return MakeError(FString::Printf(TEXT("Unknown op: %s"), *Op));
}
