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
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "SLevelViewport.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "RenderingThread.h"
#include "EnhancedInputComponent.h"
#include "Components/ActorComponent.h"
#include "Kismet2/ComponentEditorUtils.h"

FString FMCPServer::HandleModifyFunctionMetadata(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")) || !Params->HasField(TEXT("function_name")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' or 'function_name' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	const FString FunctionName = Params->GetStringField(TEXT("function_name"));

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
		if (EntryNode) break;
	}

	if (!EntryNode)
	{
		return MakeError(TEXT("Function entry node not found"));
	}

	bool bModified = false;

	// Handle BlueprintThreadSafe metadata
	if (Params->HasField(TEXT("blueprint_thread_safe")))
	{
		bool bValue = Params->GetBoolField(TEXT("blueprint_thread_safe"));
		if (bValue)
		{
			EntryNode->MetaData.bThreadSafe = true;
		}
		else
		{
			EntryNode->MetaData.bThreadSafe = false;
		}
		bModified = true;
	}

	// Handle BlueprintPure metadata
	if (Params->HasField(TEXT("blueprint_pure")))
	{
		bool bValue = Params->GetBoolField(TEXT("blueprint_pure"));
		EntryNode->MetaData.bCallInEditor = bValue;
		bModified = true;
	}

	if (!bModified)
	{
		return MakeError(TEXT("No valid metadata provided. Available metadata: blueprint_thread_safe, blueprint_pure"));
	}

	// Mark the blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Function metadata updated successfully"));
	Data->SetStringField(TEXT("function_name"), FunctionName);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleCaptureScreenshot(const TSharedPtr<FJsonObject>& Params)
{
	// Get optional filename parameter
	FString Filename = TEXT("MCP_Screenshot");
	if (Params.IsValid() && Params->HasField(TEXT("filename")))
	{
		Filename = Params->GetStringField(TEXT("filename"));
		// Remove extension if provided - we'll add .png
		Filename.RemoveFromEnd(TEXT(".png"));
		Filename.RemoveFromEnd(TEXT(".jpg"));
	}

	// Add timestamp to ensure uniqueness
	FDateTime Now = FDateTime::Now();
	FString TimestampedFilename = FString::Printf(TEXT("%s_%s"),
		*Filename,
		*Now.ToString(TEXT("%Y%m%d_%H%M%S")));

	// Construct full path: ProjectDir/Saved/Screenshots/
	FString ProjectDir = FPaths::ProjectDir();
	FString ScreenshotDir = FPaths::Combine(ProjectDir, TEXT("Saved"), TEXT("Screenshots"));

	// Ensure directory exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*ScreenshotDir))
	{
		if (!PlatformFile.CreateDirectoryTree(*ScreenshotDir))
		{
			return MakeError(FString::Printf(TEXT("Failed to create screenshot directory: %s"), *ScreenshotDir));
		}
	}

	// Construct full file path
	FString FullPath = FPaths::Combine(ScreenshotDir, TimestampedFilename + TEXT(".png"));

	// FScreenshotRequest::RequestScreenshot() only queues a request - the actual capture and PNG
	// write happen asynchronously on a later frame/thread, with no signal back to this handler, so
	// the file frequently doesn't exist yet by the time we'd return. Instead, read the viewport's
	// rendered pixels back synchronously (FViewport::ReadPixels flushes rendering commands
	// internally) and encode/write the PNG ourselves on this thread before responding.
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<SLevelViewport> ActiveViewport = LevelEditorModule.GetFirstActiveLevelViewport();
	if (!ActiveViewport.IsValid())
	{
		return MakeError(TEXT("No active level viewport available to capture"));
	}

	FLevelEditorViewportClient& ViewportClient = ActiveViewport->GetLevelViewportClient();
	FViewport* TargetViewport = ViewportClient.Viewport;
	if (!TargetViewport)
	{
		return MakeError(TEXT("Active level viewport has no renderable FViewport"));
	}

	// Force a fresh frame so we don't capture a stale buffer from before the last edit.
	TargetViewport->Draw();
	FlushRenderingCommands();

	TArray<FColor> Bitmap;
	const FIntPoint ViewportSize = TargetViewport->GetSizeXY();
	const FIntRect ViewRect(0, 0, ViewportSize.X, ViewportSize.Y);
	if (!TargetViewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), ViewRect) || Bitmap.Num() == 0)
	{
		return MakeError(TEXT("Failed to read pixels from the active viewport"));
	}

	// Viewport reads commonly come back with A=0 (no alpha channel rendered), which would encode
	// as a fully transparent PNG - force opaque so the image is actually visible.
	for (FColor& Pixel : Bitmap)
	{
		Pixel.A = 255;
	}

	TArray64<uint8> CompressedPNG;
	FImageView ImageView(Bitmap.GetData(), ViewRect.Width(), ViewRect.Height());
	FImageUtils::CompressImage(CompressedPNG, TEXT("PNG"), ImageView);

	if (CompressedPNG.Num() == 0 || !FFileHelper::SaveArrayToFile(CompressedPNG, *FullPath))
	{
		return MakeError(FString::Printf(TEXT("Failed to write screenshot file: %s"), *FullPath));
	}

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	ResponseData->SetStringField(TEXT("message"), TEXT("Screenshot captured successfully"));
	ResponseData->SetStringField(TEXT("path"), FullPath);
	ResponseData->SetStringField(TEXT("filename"), TimestampedFilename + TEXT(".png"));
	ResponseData->SetNumberField(TEXT("width"), ViewRect.Width());
	ResponseData->SetNumberField(TEXT("height"), ViewRect.Height());

	return MakeResponse(true, ResponseData);
}

FString FMCPServer::HandleRemoveErrorNodes(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_path")))
	{
		return MakeError(TEXT("Missing 'blueprint_path' parameter"));
	}

	const FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	bool bAutoRewire = Params->HasField(TEXT("auto_rewire")) ? Params->GetBoolField(TEXT("auto_rewire")) : true;

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	// First, compile to identify error nodes
	FCompilerResultsLog CompileLog;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &CompileLog);

	// Track nodes to remove
	TArray<UEdGraphNode*> NodesToRemove;
	int32 NodesRemoved = 0;
	int32 ConnectionsRewired = 0;

	// Get all graphs
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			bool bIsErrorNode = false;

			// Check for bad cast nodes (invalid target type)
			if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
			{
				// If the target class is null or invalid, this is a bad cast
				if (CastNode->TargetType == nullptr)
				{
					bIsErrorNode = true;
				}
			}

			// Check for function call nodes calling non-existent functions
			if (UK2Node_CallFunction* FunctionNode = Cast<UK2Node_CallFunction>(Node))
			{
				// Check if the function reference is valid
				UFunction* TargetFunction = FunctionNode->GetTargetFunction();
				if (!TargetFunction && FunctionNode->FunctionReference.GetMemberName() != NAME_None)
				{
					// Function is referenced but doesn't exist
					bIsErrorNode = true;
				}
			}

			// Check node's own error state
			if (Node->bHasCompilerMessage && Node->ErrorType >= EMessageSeverity::Error)
			{
				bIsErrorNode = true;
			}

			if (bIsErrorNode)
			{
				// Try to rewire execution flow around this node if requested
				if (bAutoRewire)
				{
					UEdGraphPin* ExecInputPin = nullptr;
					UEdGraphPin* ExecOutputPin = nullptr;

					// Find execution pins
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
						{
							if (Pin->Direction == EGPD_Input)
							{
								ExecInputPin = Pin;
							}
							else if (Pin->Direction == EGPD_Output)
							{
								ExecOutputPin = Pin;
							}
						}
					}

					// If we have both input and output exec pins, try to rewire
					if (ExecInputPin && ExecOutputPin && ExecInputPin->LinkedTo.Num() > 0 && ExecOutputPin->LinkedTo.Num() > 0)
					{
						UEdGraphPin* InputSource = ExecInputPin->LinkedTo[0];
						UEdGraphPin* OutputTarget = ExecOutputPin->LinkedTo[0];

						if (InputSource && OutputTarget)
						{
							InputSource->MakeLinkTo(OutputTarget);
							ConnectionsRewired++;
						}
					}
				}

				NodesToRemove.Add(Node);
			}
		}
	}

	// Remove the bad nodes. Use FBlueprintEditorUtils::RemoveNode() rather than the graph's
	// raw RemoveNode(): the raw call just drops the node from the array without breaking pin
	// links or notifying owning blueprint extensions (e.g. AnimBlueprintExtension_StateMachine
	// caches a TargetRootNode per state/transition graph). Skipping that cleanup leaves
	// dangling state that can crash the verification compile below with an internal engine
	// assertion, particularly for error nodes inside AnimGraph state machine sub-graphs.
	for (UEdGraphNode* NodeToRemove : NodesToRemove)
	{
		if (NodeToRemove && NodeToRemove->GetGraph())
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, NodeToRemove, /*bDontRecompile=*/true);
			NodesRemoved++;
		}
	}

	// Mark blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	// Recompile to verify fixes
	FCompilerResultsLog PostCompileLog;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &PostCompileLog);

	// For animation blueprints with tag errors, try removing disconnected nodes
	int32 DisconnectedNodesRemoved = 0;
	bool bHasTagErrors = false;
	for (const TSharedRef<FTokenizedMessage>& Message : PostCompileLog.Messages)
	{
		if (Message->GetSeverity() == EMessageSeverity::Error)
		{
			FString ErrorText = Message->ToText().ToString();
			if (ErrorText.Contains(TEXT("cannot find referenced node with tag")))
			{
				bHasTagErrors = true;
				break;
			}
		}
	}

	if (bHasTagErrors && Blueprint->IsA<UAnimBlueprint>())
	{
		// Find and remove disconnected nodes in animation graphs
		TArray<UEdGraph*> AnimGraphs;
		Blueprint->GetAllGraphs(AnimGraphs);

		for (UEdGraph* Graph : AnimGraphs)
		{
			if (!Graph || !Graph->GetName().Contains(TEXT("AnimGraph"))) continue;

			// Find all nodes that are disconnected (no output connections)
			TArray<UEdGraphNode*> DisconnectedNodes;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (!Node) continue;

				// Skip the root/output node
				if (Node->GetClass()->GetName().Contains(TEXT("AnimGraphNode_Root"))) continue;

				// Check if this node has any output pose connections
				bool bHasOutputConnection = false;
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin && Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
					{
						// Check if it's a pose link (not just exec or data)
						if (Pin->PinType.PinCategory == TEXT("struct") &&
							Pin->PinType.PinSubCategoryObject.IsValid() &&
							Pin->PinType.PinSubCategoryObject->GetName().Contains(TEXT("PoseLink")))
						{
							bHasOutputConnection = true;
							break;
						}
					}
				}

				// If no output pose connection, it's disconnected
				if (!bHasOutputConnection)
				{
					DisconnectedNodes.Add(Node);
				}
			}

			// Remove disconnected nodes
			for (UEdGraphNode* NodeToRemove : DisconnectedNodes)
			{
				Graph->RemoveNode(NodeToRemove);
				DisconnectedNodesRemoved++;
			}
		}

		// Recompile after removing disconnected nodes
		if (DisconnectedNodesRemoved > 0)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			FCompilerResultsLog FinalLog;
			FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &FinalLog);
			PostCompileLog = FinalLog;
		}
	}

	bool bSuccess = (Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("Error nodes removed successfully"));
	Data->SetNumberField(TEXT("nodes_removed"), NodesRemoved);
	Data->SetNumberField(TEXT("connections_rewired"), ConnectionsRewired);
	Data->SetNumberField(TEXT("disconnected_nodes_removed"), DisconnectedNodesRemoved);
	Data->SetBoolField(TEXT("compiled_successfully"), bSuccess);

	// Get remaining error count
	int32 RemainingErrors = 0;
	for (const TSharedRef<FTokenizedMessage>& Message : PostCompileLog.Messages)
	{
		if (Message->GetSeverity() == EMessageSeverity::Error)
		{
			RemainingErrors++;
		}
	}
	Data->SetNumberField(TEXT("remaining_errors"), RemainingErrors);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleClearAnimationBlueprintTags(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));
	bool bRemoveExtension = Params->HasField(TEXT("remove_extension")) ? Params->GetBoolField(TEXT("remove_extension")) : false;

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
	if (!AnimBP)
	{
		return MakeError(TEXT("Blueprint is not an Animation Blueprint"));
	}

	TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);
	int32 RemovedCount = 0;
	int32 ExtensionsRemoved = 0;

	// Find AnimBlueprintExtension_Tag objects
	TArray<UBlueprintExtension*> ExtensionsToRemove;
	TArray<FString> AllExtensions;

	for (UBlueprintExtension* Extension : AnimBP->GetExtensions())
	{
		if (Extension)
		{
			FString ClassName = Extension->GetClass()->GetName();
			AllExtensions.Add(ClassName);
			if (ClassName.Contains(TEXT("Tag")))
			{
				ExtensionsToRemove.Add(Extension);
			}
		}
	}

	// Output all extension names for debugging
	TArray<TSharedPtr<FJsonValue>> ExtArray;
	for (const FString& ExtName : AllExtensions)
	{
		ExtArray.Add(MakeShared<FJsonValueString>(ExtName));
	}
	Data->SetArrayField(TEXT("all_extensions"), ExtArray);
	Data->SetNumberField(TEXT("tag_extensions_found"), ExtensionsToRemove.Num());

	// If remove_extension is true, actually remove the extension from the blueprint
	if (bRemoveExtension && ExtensionsToRemove.Num() > 0)
	{
		AnimBP->Modify();

		// Get direct access to the extensions array via reflection
		FArrayProperty* ExtensionsProp = FindFProperty<FArrayProperty>(UBlueprint::StaticClass(), TEXT("Extensions"));
		if (ExtensionsProp)
		{
			void* ArrayPtr = ExtensionsProp->ContainerPtrToValuePtr<void>(AnimBP);
			FScriptArrayHelper ArrayHelper(ExtensionsProp, ArrayPtr);

			// Remove extensions in reverse order to maintain indices
			for (int32 i = ArrayHelper.Num() - 1; i >= 0; i--)
			{
				if (ArrayHelper.IsValidIndex(i))
				{
					FObjectProperty* InnerProp = CastField<FObjectProperty>(ExtensionsProp->Inner);
					if (InnerProp)
					{
						UObject* ExtObj = InnerProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(i));
						if (ExtObj && ExtObj->GetClass()->GetName().Contains(TEXT("Tag")))
						{
							ArrayHelper.RemoveValues(i, 1);
							ExtensionsRemoved++;
						}
					}
				}
			}
		}

		Data->SetNumberField(TEXT("extensions_removed"), ExtensionsRemoved);

		if (ExtensionsRemoved > 0)
		{
			AnimBP->MarkPackageDirty();

			// Save immediately
			UPackage* Package = AnimBP->GetOutermost();
			FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			bool bSaved = UPackage::SavePackage(Package, AnimBP, *PackageFilename, SaveArgs);

			if (bSaved)
			{
				Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Removed %d tag extension(s) and saved. Restart editor to reload."), ExtensionsRemoved));
			}
			else
			{
				Data->SetStringField(TEXT("message"), TEXT("Extensions removed but save failed."));
			}

			return MakeResponse(true, Data);
		}
	}

	// Otherwise, just clear the data inside the extensions
	TArray<FString> FoundProperties;

	for (UBlueprintExtension* Extension : ExtensionsToRemove)
	{
		// Iterate all properties to find tag-related data
		for (TFieldIterator<FProperty> PropIt(Extension->GetClass()); PropIt; ++PropIt)
		{
			FProperty* Prop = *PropIt;
			FString PropInfo = FString::Printf(TEXT("%s (%s)"), *Prop->GetName(), *Prop->GetClass()->GetName());
			FoundProperties.Add(PropInfo);

			// Try map properties
			if (FMapProperty* MapProp = CastField<FMapProperty>(Prop))
			{
				void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(Extension);
				FScriptMapHelper MapHelper(MapProp, MapPtr);
				int32 NumEntries = MapHelper.Num();
				if (NumEntries > 0)
				{
					MapHelper.EmptyValues();
					RemovedCount += NumEntries;
				}
			}
			// Try array properties
			else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
			{
				void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<void>(Extension);
				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);
				int32 NumEntries = ArrayHelper.Num();
				if (NumEntries > 0)
				{
					ArrayHelper.EmptyValues();
					RemovedCount += NumEntries;
				}
			}
			// Try struct properties - dig into them to find maps/arrays
			else if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
			{
				FoundProperties.Add(FString::Printf(TEXT("  -> Struct: %s"), *StructProp->Struct->GetName()));

				// Get pointer to the struct data
				void* StructPtr = StructProp->ContainerPtrToValuePtr<void>(Extension);

				// Iterate properties inside the struct
				for (TFieldIterator<FProperty> StructPropIt(StructProp->Struct); StructPropIt; ++StructPropIt)
				{
					FProperty* InnerProp = *StructPropIt;
					FString InnerPropInfo = FString::Printf(TEXT("    -> %s (%s)"), *InnerProp->GetName(), *InnerProp->GetClass()->GetName());
					FoundProperties.Add(InnerPropInfo);

					// Check for maps inside struct
					if (FMapProperty* InnerMapProp = CastField<FMapProperty>(InnerProp))
					{
						void* MapPtr = InnerMapProp->ContainerPtrToValuePtr<void>(StructPtr);
						FScriptMapHelper MapHelper(InnerMapProp, MapPtr);
						int32 NumEntries = MapHelper.Num();
						FoundProperties.Add(FString::Printf(TEXT("      Map entries: %d"), NumEntries));
						if (NumEntries > 0)
						{
							MapHelper.EmptyValues();
							RemovedCount += NumEntries;
						}
					}
					// Check for arrays inside struct
					else if (FArrayProperty* InnerArrayProp = CastField<FArrayProperty>(InnerProp))
					{
						void* ArrayPtr = InnerArrayProp->ContainerPtrToValuePtr<void>(StructPtr);
						FScriptArrayHelper ArrayHelper(InnerArrayProp, ArrayPtr);
						int32 NumEntries = ArrayHelper.Num();
						FoundProperties.Add(FString::Printf(TEXT("      Array entries: %d"), NumEntries));
						if (NumEntries > 0)
						{
							ArrayHelper.EmptyValues();
							RemovedCount += NumEntries;
						}
					}
				}
			}
		}
	}

	// Store found properties for debugging
	TArray<TSharedPtr<FJsonValue>> PropsArray;
	for (const FString& PropInfo : FoundProperties)
	{
		PropsArray.Add(MakeShared<FJsonValueString>(PropInfo));
	}
	Data->SetArrayField(TEXT("found_properties"), PropsArray);

	if (RemovedCount > 0)
	{
		AnimBP->MarkPackageDirty();

		// Save immediately to persist changes before any compilation attempt
		UPackage* Package = AnimBP->GetOutermost();
		FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		bool bSaved = UPackage::SavePackage(Package, AnimBP, *PackageFilename, SaveArgs);

		if (bSaved)
		{
			Data->SetStringField(TEXT("message"), TEXT("Tag mappings cleared and saved. Restart the editor to reload cleanly."));
		}
		else
		{
			Data->SetStringField(TEXT("message"), TEXT("Tag mappings cleared but save failed. Try saving manually."));
		}
	}
	else
	{
		Data->SetStringField(TEXT("message"), TEXT("No tag mappings found to clear. Try with remove_extension: true to fully remove the tag extension."));
	}

	Data->SetNumberField(TEXT("removed_count"), RemovedCount);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleClearAnimGraph(const TSharedPtr<FJsonObject>& Params)
{
	FString BlueprintPath = Params->GetStringField(TEXT("blueprint_path"));

	UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
	}

	UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Blueprint);
	if (!AnimBP)
	{
		return MakeError(TEXT("Blueprint is not an Animation Blueprint"));
	}

	// Find the AnimGraph - check both FunctionGraphs and UbergraphPages
	UEdGraph* AnimGraph = nullptr;

	// First check FunctionGraphs (where AnimGraph is typically stored)
	for (UEdGraph* Graph : AnimBP->FunctionGraphs)
	{
		if (Graph && Graph->GetName() == TEXT("AnimGraph"))
		{
			AnimGraph = Graph;
			break;
		}
	}

	// If not found, check UbergraphPages
	if (!AnimGraph)
	{
		for (UEdGraph* Graph : AnimBP->UbergraphPages)
		{
			if (Graph && Graph->GetName() == TEXT("AnimGraph"))
			{
				AnimGraph = Graph;
				break;
			}
		}
	}

	if (!AnimGraph)
	{
		return MakeError(TEXT("AnimGraph not found in animation blueprint"));
	}

	TSharedPtr<FJsonObject> Data = MakeShareable(new FJsonObject);
	int32 DeletedCount = 0;

	// Collect all nodes except the root output node
	TArray<UEdGraphNode*> NodesToDelete;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (Node && !Node->GetClass()->GetName().Contains(TEXT("AnimGraphNode_Root")))
		{
			NodesToDelete.Add(Node);
		}
	}

	// Delete the nodes
	for (UEdGraphNode* Node : NodesToDelete)
	{
		AnimGraph->RemoveNode(Node);
		DeletedCount++;
	}

	// Also clear all nested/sub-graphs (state machines, transitions, etc.)
	// This prevents crashes from orphaned nodes in nested graphs
	TArray<UEdGraph*> AllGraphs;
	AllGraphs.Append(AnimBP->FunctionGraphs);
	AllGraphs.Append(AnimBP->UbergraphPages);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph && Graph != AnimGraph)
		{
			// Clear all nodes from this sub-graph
			TArray<UEdGraphNode*> SubGraphNodesToDelete;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node)
				{
					SubGraphNodesToDelete.Add(Node);
				}
			}

			for (UEdGraphNode* Node : SubGraphNodesToDelete)
			{
				Graph->RemoveNode(Node);
				DeletedCount++;
			}
		}
	}

	if (DeletedCount > 0)
	{
		AnimBP->MarkPackageDirty();
		Data->SetStringField(TEXT("message"), FString::Printf(TEXT("Deleted %d nodes from AnimGraph and all nested graphs (state machines, transitions, etc.). Blueprint marked dirty. Compile manually in the editor."), DeletedCount));
	}
	else
	{
		Data->SetStringField(TEXT("message"), TEXT("No nodes to delete (only root output node found)."));
	}

	Data->SetNumberField(TEXT("deleted_count"), DeletedCount);

	return MakeResponse(true, Data);
}

// ==================================================
// Sprint 1: Blueprint Function Creation Commands
// ==================================================
