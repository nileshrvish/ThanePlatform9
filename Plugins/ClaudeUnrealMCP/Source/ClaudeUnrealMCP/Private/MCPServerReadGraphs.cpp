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
#include "MCPServerHelpers.h"

FString FMCPServer::HandleReadEventGraph(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	FString Path = Params->GetStringField(TEXT("path"));
	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);

	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
	}

	TArray<TSharedPtr<FJsonValue>> GraphsArray;

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph) continue;

		TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
		GraphObj->SetStringField(TEXT("name"), Graph->GetName());

		TArray<TSharedPtr<FJsonValue>> NodesArray;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;

			TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
			NodeObj->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
			NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
			NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

			// Get node-specific info
			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("Event"));
				NodeObj->SetStringField(TEXT("event_name"), EventNode->GetFunctionName().ToString());
			}
			else if (UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("FunctionCall"));
				NodeObj->SetStringField(TEXT("function_name"), FuncNode->GetFunctionName().ToString());
			}
			else if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("VariableGet"));
				NodeObj->SetStringField(TEXT("variable_name"), GetNode->GetVarName().ToString());
			}
			else if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("VariableSet"));
				NodeObj->SetStringField(TEXT("variable_name"), SetNode->GetVarName().ToString());
			}
			else
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("Other"));
			}

			// Pin defaults (useful for nodes like Delay)
			TArray<TSharedPtr<FJsonValue>> PinsArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;

				TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
				PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
				PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));

				TSharedPtr<FJsonObject> PinTypeObj = MakeShared<FJsonObject>();
				SerializePinType(Pin->PinType, PinTypeObj);
				PinObj->SetObjectField(TEXT("type"), PinTypeObj);

				if (!Pin->DefaultValue.IsEmpty())
				{
					PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
				}

				if (!Pin->DefaultTextValue.IsEmpty())
				{
					PinObj->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
				}

				if (Pin->DefaultObject)
				{
					PinObj->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
				}

				PinObj->SetBoolField(TEXT("is_linked"), Pin->LinkedTo.Num() > 0);

				PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
			}
			NodeObj->SetArrayField(TEXT("pins"), PinsArray);

			// Get pin connections
			TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output) continue;

				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin || !LinkedPin->GetOwningNode()) continue;

					TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
					ConnObj->SetStringField(TEXT("from_pin"), Pin->PinName.ToString());
					ConnObj->SetStringField(TEXT("to_node"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
					ConnObj->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
					ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
				}
			}
			NodeObj->SetArrayField(TEXT("connections"), ConnectionsArray);

			NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
		}

		GraphObj->SetArrayField(TEXT("nodes"), NodesArray);
		GraphObj->SetNumberField(TEXT("node_count"), NodesArray.Num());
		GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("graphs"), GraphsArray);
	Data->SetNumberField(TEXT("count"), GraphsArray.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadEventGraphDetailed(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	FString Path = Params->GetStringField(TEXT("path"));
	const int32 MaxNodes = Params->HasField(TEXT("max_nodes")) ? Params->GetIntegerField(TEXT("max_nodes")) : -1;
	const int32 StartIndex = Params->HasField(TEXT("start_index")) ? Params->GetIntegerField(TEXT("start_index")) : 0;
	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);

	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
	}

	TArray<TSharedPtr<FJsonValue>> GraphsArray;

	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph) continue;

		TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
		GraphObj->SetStringField(TEXT("name"), Graph->GetName());

		TArray<TSharedPtr<FJsonValue>> NodesArray;
		int32 NodeIndex = 0;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			if (NodeIndex++ < StartIndex) continue;
			if (MaxNodes >= 0 && NodesArray.Num() >= MaxNodes) break;

			TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
			NodeObj->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
			NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
			NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("Event"));
				NodeObj->SetStringField(TEXT("event_name"), EventNode->GetFunctionName().ToString());
			}
			else if (UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("FunctionCall"));
				NodeObj->SetStringField(TEXT("function_name"), FuncNode->GetFunctionName().ToString());
			}
			else if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("VariableGet"));
				NodeObj->SetStringField(TEXT("variable_name"), GetNode->GetVarName().ToString());
			}
			else if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("VariableSet"));
				NodeObj->SetStringField(TEXT("variable_name"), SetNode->GetVarName().ToString());
			}
			else
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("Other"));
			}

			TArray<TSharedPtr<FJsonValue>> PinsArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;

				TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
				PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
				PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));

				TSharedPtr<FJsonObject> PinTypeObj = MakeShared<FJsonObject>();
				SerializePinType(Pin->PinType, PinTypeObj);
				PinObj->SetObjectField(TEXT("type"), PinTypeObj);

				if (!Pin->DefaultValue.IsEmpty())
				{
					PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
				}

				if (!Pin->DefaultTextValue.IsEmpty())
				{
					PinObj->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
				}

				if (Pin->DefaultObject)
				{
					PinObj->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
				}

				PinObj->SetBoolField(TEXT("is_linked"), Pin->LinkedTo.Num() > 0);

				PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
			}
			NodeObj->SetArrayField(TEXT("pins"), PinsArray);

			TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output) continue;

				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin || !LinkedPin->GetOwningNode()) continue;

					TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
					ConnObj->SetStringField(TEXT("from_pin"), Pin->PinName.ToString());
					ConnObj->SetStringField(TEXT("to_node"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
					ConnObj->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
					ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
				}
			}
			NodeObj->SetArrayField(TEXT("connections"), ConnectionsArray);

			NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
		}

		GraphObj->SetArrayField(TEXT("nodes"), NodesArray);
		GraphObj->SetNumberField(TEXT("node_count"), NodesArray.Num());
		GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("graphs"), GraphsArray);
	Data->SetNumberField(TEXT("count"), GraphsArray.Num());

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadFunctionGraphs(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	FString Path = Params->GetStringField(TEXT("path"));
	const FString FilterName = Params->HasField(TEXT("name")) ? Params->GetStringField(TEXT("name")) : FString();
	const int32 MaxNodes = Params->HasField(TEXT("max_nodes")) ? Params->GetIntegerField(TEXT("max_nodes")) : -1;
	const int32 StartIndex = Params->HasField(TEXT("start_index")) ? Params->GetIntegerField(TEXT("start_index")) : 0;
	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);

	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
	}

	TArray<TSharedPtr<FJsonValue>> GraphsArray;

	// Collect all function graphs: regular + interface implementation graphs
	TArray<UEdGraph*> AllFunctionGraphs;
	AllFunctionGraphs.Append(Blueprint->FunctionGraphs);
	for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
	{
		AllFunctionGraphs.Append(Interface.Graphs);
	}

	for (UEdGraph* Graph : AllFunctionGraphs)
	{
		if (!Graph) continue;
		if (!FilterName.IsEmpty() && Graph->GetName() != FilterName) continue;

		TSharedPtr<FJsonObject> GraphObj = MakeShared<FJsonObject>();
		GraphObj->SetStringField(TEXT("name"), Graph->GetName());
		GraphObj->SetStringField(TEXT("graph_type"), TEXT("Function"));

		TArray<TSharedPtr<FJsonValue>> NodesArray;
		int32 NodeIndex = 0;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node) continue;
			if (NodeIndex++ < StartIndex) continue;
			if (MaxNodes >= 0 && NodesArray.Num() >= MaxNodes) break;

			TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
			NodeObj->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
			NodeObj->SetStringField(TEXT("class"), Node->GetClass()->GetName());
			NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("Event"));
				NodeObj->SetStringField(TEXT("event_name"), EventNode->GetFunctionName().ToString());
			}
			else if (UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("FunctionCall"));
				NodeObj->SetStringField(TEXT("function_name"), FuncNode->GetFunctionName().ToString());
			}
			else if (UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("VariableGet"));
				NodeObj->SetStringField(TEXT("variable_name"), GetNode->GetVarName().ToString());
			}
			else if (UK2Node_VariableSet* SetNode = Cast<UK2Node_VariableSet>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("VariableSet"));
				NodeObj->SetStringField(TEXT("variable_name"), SetNode->GetVarName().ToString());
			}
			else if (Cast<UK2Node_FunctionEntry>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("FunctionEntry"));
			}
			else if (Cast<UK2Node_FunctionResult>(Node))
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("FunctionResult"));
			}
			else
			{
				NodeObj->SetStringField(TEXT("type"), TEXT("Other"));
			}

			// For K2Node_PropertyAccess, extract the property path via reflection
			if (Node->GetClass()->GetName() == TEXT("K2Node_PropertyAccess"))
			{
				// Get TextPath (FText) via reflection - avoids needing private header include
				FTextProperty* TextPathProp = CastField<FTextProperty>(Node->GetClass()->FindPropertyByName(TEXT("TextPath")));
				if (TextPathProp)
				{
					const FText& PathText = TextPathProp->GetPropertyValue_InContainer(Node);
					if (!PathText.IsEmpty())
					{
						NodeObj->SetStringField(TEXT("property_path"), PathText.ToString());
					}
				}
				// Also get the Path array (TArray<FString>) for segment-level detail
				FArrayProperty* PathArrayProp = CastField<FArrayProperty>(Node->GetClass()->FindPropertyByName(TEXT("Path")));
				if (PathArrayProp)
				{
					FScriptArrayHelper ArrayHelper(PathArrayProp, PathArrayProp->ContainerPtrToValuePtr<void>(Node));
					TArray<TSharedPtr<FJsonValue>> PathSegments;
					FStrProperty* InnerProp = CastField<FStrProperty>(PathArrayProp->Inner);
					if (InnerProp)
					{
						for (int32 i = 0; i < ArrayHelper.Num(); i++)
						{
							FString Segment = InnerProp->GetPropertyValue(ArrayHelper.GetRawPtr(i));
							PathSegments.Add(MakeShared<FJsonValueString>(Segment));
						}
					}
					if (PathSegments.Num() > 0)
					{
						NodeObj->SetArrayField(TEXT("path_segments"), PathSegments);
					}
				}
			}

			TArray<TSharedPtr<FJsonValue>> PinsArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin) continue;

				TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
				PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
				PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));

				TSharedPtr<FJsonObject> PinTypeObj = MakeShared<FJsonObject>();
				SerializePinType(Pin->PinType, PinTypeObj);
				PinObj->SetObjectField(TEXT("type"), PinTypeObj);

				if (!Pin->DefaultValue.IsEmpty())
				{
					PinObj->SetStringField(TEXT("default_value"), Pin->DefaultValue);
				}

				if (!Pin->DefaultTextValue.IsEmpty())
				{
					PinObj->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
				}

				if (Pin->DefaultObject)
				{
					PinObj->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
				}

				PinObj->SetBoolField(TEXT("is_linked"), Pin->LinkedTo.Num() > 0);

				PinsArray.Add(MakeShared<FJsonValueObject>(PinObj));
			}
			NodeObj->SetArrayField(TEXT("pins"), PinsArray);

			TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output) continue;

				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin || !LinkedPin->GetOwningNode()) continue;

					TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
					ConnObj->SetStringField(TEXT("from_pin"), Pin->PinName.ToString());
					ConnObj->SetStringField(TEXT("to_node"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
					ConnObj->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
					ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
				}
			}
			NodeObj->SetArrayField(TEXT("connections"), ConnectionsArray);

			NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
		}

		GraphObj->SetArrayField(TEXT("nodes"), NodesArray);
		GraphObj->SetNumberField(TEXT("node_count"), NodesArray.Num());
		GraphsArray.Add(MakeShared<FJsonValueObject>(GraphObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("graphs"), GraphsArray);
	Data->SetNumberField(TEXT("count"), GraphsArray.Num());

	return MakeResponse(true, Data);
}

static TArray<TSharedPtr<FJsonValue>> SerializeRichCurveKeys(const FRichCurve& Curve)
{
	TArray<TSharedPtr<FJsonValue>> KeysArray;
	const TArray<FRichCurveKey> Keys = Curve.GetCopyOfKeys();
	for (const FRichCurveKey& Key : Keys)
	{
		TSharedPtr<FJsonObject> KeyObj = MakeShared<FJsonObject>();
		KeyObj->SetNumberField(TEXT("time"), Key.Time);
		KeyObj->SetNumberField(TEXT("value"), Key.Value);
		KeyObj->SetNumberField(TEXT("arrive_tangent"), Key.ArriveTangent);
		KeyObj->SetNumberField(TEXT("leave_tangent"), Key.LeaveTangent);
		KeyObj->SetNumberField(TEXT("arrive_tangent_weight"), Key.ArriveTangentWeight);
		KeyObj->SetNumberField(TEXT("leave_tangent_weight"), Key.LeaveTangentWeight);
		KeyObj->SetStringField(TEXT("interp_mode"), UEnum::GetValueAsString(Key.InterpMode));
		KeyObj->SetStringField(TEXT("tangent_mode"), UEnum::GetValueAsString(Key.TangentMode));
		KeyObj->SetStringField(TEXT("tangent_weight_mode"), UEnum::GetValueAsString(Key.TangentWeightMode));
		KeysArray.Add(MakeShared<FJsonValueObject>(KeyObj));
	}

	return KeysArray;
}

FString FMCPServer::HandleReadTimelines(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid() || !Params->HasField(TEXT("path")))
	{
		return MakeError(TEXT("Missing 'path' parameter"));
	}

	const FString Path = Params->GetStringField(TEXT("path"));
	UBlueprint* Blueprint = LoadBlueprintFromPath(Path);
	if (!Blueprint)
	{
		return MakeError(FString::Printf(TEXT("Blueprint not found: %s"), *Path));
	}

	TArray<TSharedPtr<FJsonValue>> TimelinesArray;

	for (UTimelineTemplate* Timeline : Blueprint->Timelines)
	{
		if (!Timeline) continue;

		TSharedPtr<FJsonObject> TimelineObj = MakeShared<FJsonObject>();
		TimelineObj->SetStringField(TEXT("name"), Timeline->GetVariableName().ToString());
		TimelineObj->SetStringField(TEXT("update_function"), Timeline->GetUpdateFunctionName().ToString());
		TimelineObj->SetStringField(TEXT("finished_function"), Timeline->GetFinishedFunctionName().ToString());
		TimelineObj->SetNumberField(TEXT("length"), Timeline->TimelineLength);
		TimelineObj->SetStringField(TEXT("length_mode"), UEnum::GetValueAsString(Timeline->LengthMode));
		TimelineObj->SetBoolField(TEXT("auto_play"), Timeline->bAutoPlay);
		TimelineObj->SetBoolField(TEXT("loop"), Timeline->bLoop);
		TimelineObj->SetBoolField(TEXT("replicated"), Timeline->bReplicated);
		TimelineObj->SetBoolField(TEXT("ignore_time_dilation"), Timeline->bIgnoreTimeDilation);
		TimelineObj->SetStringField(TEXT("tick_group"), UEnum::GetValueAsString(Timeline->TimelineTickGroup));

		TArray<TSharedPtr<FJsonValue>> EventTracksArray;
		for (const FTTEventTrack& Track : Timeline->EventTracks)
		{
			TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
			TrackObj->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
			TrackObj->SetStringField(TEXT("function_name"), Track.GetFunctionName().ToString());
			TrackObj->SetBoolField(TEXT("is_external_curve"), Track.bIsExternalCurve);
			if (Track.CurveKeys)
			{
				TrackObj->SetArrayField(TEXT("keys"), SerializeRichCurveKeys(Track.CurveKeys->FloatCurve));
			}
			EventTracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
		}
		TimelineObj->SetArrayField(TEXT("event_tracks"), EventTracksArray);

		TArray<TSharedPtr<FJsonValue>> FloatTracksArray;
		for (const FTTFloatTrack& Track : Timeline->FloatTracks)
		{
			TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
			TrackObj->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
			TrackObj->SetStringField(TEXT("property_name"), Track.GetPropertyName().ToString());
			TrackObj->SetBoolField(TEXT("is_external_curve"), Track.bIsExternalCurve);
			if (Track.CurveFloat)
			{
				TrackObj->SetArrayField(TEXT("keys"), SerializeRichCurveKeys(Track.CurveFloat->FloatCurve));
			}
			FloatTracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
		}
		TimelineObj->SetArrayField(TEXT("float_tracks"), FloatTracksArray);

		TArray<TSharedPtr<FJsonValue>> VectorTracksArray;
		for (const FTTVectorTrack& Track : Timeline->VectorTracks)
		{
			TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
			TrackObj->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
			TrackObj->SetStringField(TEXT("property_name"), Track.GetPropertyName().ToString());
			TrackObj->SetBoolField(TEXT("is_external_curve"), Track.bIsExternalCurve);
			if (Track.CurveVector)
			{
				TSharedPtr<FJsonObject> KeysObj = MakeShared<FJsonObject>();
				KeysObj->SetArrayField(TEXT("x"), SerializeRichCurveKeys(Track.CurveVector->FloatCurves[0]));
				KeysObj->SetArrayField(TEXT("y"), SerializeRichCurveKeys(Track.CurveVector->FloatCurves[1]));
				KeysObj->SetArrayField(TEXT("z"), SerializeRichCurveKeys(Track.CurveVector->FloatCurves[2]));
				TrackObj->SetObjectField(TEXT("keys"), KeysObj);
			}
			VectorTracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
		}
		TimelineObj->SetArrayField(TEXT("vector_tracks"), VectorTracksArray);

		TArray<TSharedPtr<FJsonValue>> LinearColorTracksArray;
		for (const FTTLinearColorTrack& Track : Timeline->LinearColorTracks)
		{
			TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
			TrackObj->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
			TrackObj->SetStringField(TEXT("property_name"), Track.GetPropertyName().ToString());
			TrackObj->SetBoolField(TEXT("is_external_curve"), Track.bIsExternalCurve);
			if (Track.CurveLinearColor)
			{
				TSharedPtr<FJsonObject> KeysObj = MakeShared<FJsonObject>();
				KeysObj->SetArrayField(TEXT("r"), SerializeRichCurveKeys(Track.CurveLinearColor->FloatCurves[0]));
				KeysObj->SetArrayField(TEXT("g"), SerializeRichCurveKeys(Track.CurveLinearColor->FloatCurves[1]));
				KeysObj->SetArrayField(TEXT("b"), SerializeRichCurveKeys(Track.CurveLinearColor->FloatCurves[2]));
				KeysObj->SetArrayField(TEXT("a"), SerializeRichCurveKeys(Track.CurveLinearColor->FloatCurves[3]));
				TrackObj->SetObjectField(TEXT("keys"), KeysObj);
			}
			LinearColorTracksArray.Add(MakeShared<FJsonValueObject>(TrackObj));
		}
		TimelineObj->SetArrayField(TEXT("linear_color_tracks"), LinearColorTracksArray);

		TimelinesArray.Add(MakeShared<FJsonValueObject>(TimelineObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("timelines"), TimelinesArray);
	Data->SetNumberField(TEXT("count"), TimelinesArray.Num());

	return MakeResponse(true, Data);
}

