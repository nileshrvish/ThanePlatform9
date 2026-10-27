#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Common/TcpListener.h"

class FMCPServer
{
public:
	FMCPServer();
	~FMCPServer();

	bool Start(int32 Port = 9877);
	void Stop();
private:
	bool HandleConnection(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint);
	void HandleClient(FSocket* ClientSocket);
	FString ProcessCommand(const TSharedPtr<FJsonObject>& JsonCommand);

	// Blueprint reading commands
	FString HandleListBlueprints(const TSharedPtr<FJsonObject>& Params);
	FString HandleCheckAllBlueprints(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadBlueprint(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadVariables(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadClassDefaults(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadComponents(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadComponentProperties(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadEventGraph(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadEventGraphDetailed(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadFunctionGraphs(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadTimelines(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadInterface(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadUserDefinedStruct(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadUserDefinedEnum(const TSharedPtr<FJsonObject>& Params);
	FString HandleListActors(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadActorComponents(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadActorComponentProperties(const TSharedPtr<FJsonObject>& Params);
	FString HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params);
	FString HandleGetActorMaterialInfo(const TSharedPtr<FJsonObject>& Params);
	FString HandleGetSceneSummary(const TSharedPtr<FJsonObject>& Params);

	// Blueprint writing commands
	FString HandleAddComponent(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetBlueprintCDOClassReference(const TSharedPtr<FJsonObject>& Params);
	FString HandleReplaceComponentMapValue(const TSharedPtr<FJsonObject>& Params);
	FString HandleReplaceBlueprintArrayValue(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddInputMapping(const TSharedPtr<FJsonObject>& Params);
	FString HandleRemoveInputMapping(const TSharedPtr<FJsonObject>& Params);
	FString HandleReparentBlueprint(const TSharedPtr<FJsonObject>& Params);
	FString HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params);
	FString HandleSaveAsset(const TSharedPtr<FJsonObject>& Params);
	FString HandleSaveAll(const TSharedPtr<FJsonObject>& Params);
	FString HandleDeleteInterfaceFunction(const TSharedPtr<FJsonObject>& Params);
	FString HandleModifyInterfaceFunctionParameter(const TSharedPtr<FJsonObject>& Params);
	FString HandleDeleteFunctionGraph(const TSharedPtr<FJsonObject>& Params);
	FString HandleClearEventGraph(const TSharedPtr<FJsonObject>& Params);
	FString HandleRefreshNodes(const TSharedPtr<FJsonObject>& Params);
	FString HandleBreakOrphanedPins(const TSharedPtr<FJsonObject>& Params);
	FString HandleDeleteUserDefinedStruct(const TSharedPtr<FJsonObject>& Params);
	FString HandleModifyStructField(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetBlueprintCompileSettings(const TSharedPtr<FJsonObject>& Params);
	FString HandleModifyFunctionMetadata(const TSharedPtr<FJsonObject>& Params);
	FString HandleCaptureScreenshot(const TSharedPtr<FJsonObject>& Params);
	FString HandleRemoveErrorNodes(const TSharedPtr<FJsonObject>& Params);
	FString HandleClearAnimationBlueprintTags(const TSharedPtr<FJsonObject>& Params);
	FString HandleClearAnimGraph(const TSharedPtr<FJsonObject>& Params);

	// Blueprint function creation commands (Sprint 1)
	FString HandleCreateBlueprintFunction(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddFunctionInput(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddFunctionOutput(const TSharedPtr<FJsonObject>& Params);
	FString HandleRenameBlueprintFunction(const TSharedPtr<FJsonObject>& Params);

	// Level actor property commands (Sprint 2)
	FString HandleReadActorProperties(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetActorProperties(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetActorComponentProperty(const TSharedPtr<FJsonObject>& Params);
	FString HandleReconstructActor(const TSharedPtr<FJsonObject>& Params);

	// Component map value manipulation (Sprint 3)
	FString HandleClearComponentMapValueArray(const TSharedPtr<FJsonObject>& Params);
	FString HandleReplaceComponentClass(const TSharedPtr<FJsonObject>& Params);
	FString HandleDeleteComponent(const TSharedPtr<FJsonObject>& Params);

	// Blueprint CDO property manipulation (Sprint 4)
	FString HandleSetBlueprintCDOProperty(const TSharedPtr<FJsonObject>& Params);
	FString HandleRemoveImplementedInterface(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddImplementedInterface(const TSharedPtr<FJsonObject>& Params);
	FString HandleMigrateInterfaceReferences(const TSharedPtr<FJsonObject>& Params);

	// Blueprint node manipulation (Sprint 5)
	FString HandleConnectNodes(const TSharedPtr<FJsonObject>& Params);
	FString HandleDisconnectPin(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddSetStructNode(const TSharedPtr<FJsonObject>& Params);
	FString HandleDeleteNode(const TSharedPtr<FJsonObject>& Params);

	// Input system reading (Sprint 6)
	FString HandleReadInputMappingContext(const TSharedPtr<FJsonObject>& Params);

	// Struct migration (Sprint 7)
	FString HandleMigrateStructReferences(const TSharedPtr<FJsonObject>& Params);
	FString HandleMigrateEnumReferences(const TSharedPtr<FJsonObject>& Params);
	FString HandleFixPropertyAccessPaths(const TSharedPtr<FJsonObject>& Params);
	FString HandleCleanPropertyAccessPaths(const TSharedPtr<FJsonObject>& Params);
	FString HandleFixStructSubPins(const TSharedPtr<FJsonObject>& Params);
	FString HandleRenameLocalVariable(const TSharedPtr<FJsonObject>& Params);
	FString HandleFixPinEnumType(const TSharedPtr<FJsonObject>& Params);
	FString HandleFixEnumDefaults(const TSharedPtr<FJsonObject>& Params);
	FString HandleForceFixEnumPinDefaults(const TSharedPtr<FJsonObject>& Params);
	FString HandleFixAssetStructReference(const TSharedPtr<FJsonObject>& Params);
	FString HandleReconstructNode(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetPinDefault(const TSharedPtr<FJsonObject>& Params);
	FString HandleRestoreStructNodePins(const TSharedPtr<FJsonObject>& Params);
	FString HandleFixStructEnumFieldDefaults(const TSharedPtr<FJsonObject>& Params);
	FString HandleFixOptionalStructPinDefaults(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetStructFieldDefault(const TSharedPtr<FJsonObject>& Params);

	// Chooser Table migration (Sprint 9)
	FString HandleMigrateChooserTable(const TSharedPtr<FJsonObject>& Params);

	// Generic asset ops (Sprint 10)
	FString HandleRunPython(const TSharedPtr<FJsonObject>& Params);

	// PCG, GAS, Networking, Volumes (Sprint 28-31)
	FString HandlePCGOps(const TSharedPtr<FJsonObject>& Params);
	FString HandleCreateGameplayAbility(const TSharedPtr<FJsonObject>& Params);
	FString HandleCreateGameplayEffect(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetReplication(const TSharedPtr<FJsonObject>& Params);
	FString HandleCreateVolume(const TSharedPtr<FJsonObject>& Params);
	FString HandleCreateProceduralMesh(const TSharedPtr<FJsonObject>& Params);

	// Animation Authoring (Sprint 27)
	FString HandleCreateMontage(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddMontageSection(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadMontage(const TSharedPtr<FJsonObject>& Params);
	FString HandleCreateBlendSpace(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddBlendSpaceSample(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadAnimSequence(const TSharedPtr<FJsonObject>& Params);
	FString HandleCreateAnimBlueprint(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetupFpSpinePitchAbp(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddSocket(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetSocketTransform(const TSharedPtr<FJsonObject>& Params);
	FString HandleListSockets(const TSharedPtr<FJsonObject>& Params);
	FString HandleRetargetAnimAsset(const TSharedPtr<FJsonObject>& Params);
	FString HandleCompareAnimBonePose(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetAnimCurveKeys(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetAnimNotifyProperty(const TSharedPtr<FJsonObject>& Params);
	FString HandleImportTexture(const TSharedPtr<FJsonObject>& Params);
	FString HandleMoveActor(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadStateMachine(const TSharedPtr<FJsonObject>& Params);
	FString HandleDuplicateStateTransition(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddEnumOrConditionToTransitionRule(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddStaticMeshSocket(const TSharedPtr<FJsonObject>& Params);
	FString HandleListStaticMeshSockets(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetStaticMeshSocketTransform(const TSharedPtr<FJsonObject>& Params);

	// State Trees + Audio + Landscape (Sprint 24-26)
	FString HandleCreateStateTree(const TSharedPtr<FJsonObject>& Params);
	FString HandleCreateSoundCue(const TSharedPtr<FJsonObject>& Params);
	FString HandleCreateSoundAttenuation(const TSharedPtr<FJsonObject>& Params);
	FString HandleGetLandscapeInfo(const TSharedPtr<FJsonObject>& Params);

	// GameplayTags + Splines + Physics (Sprint 21-23)
	FString HandleManageGameplayTags(const TSharedPtr<FJsonObject>& Params);
	FString HandleSplineOps(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetPhysics(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetCollision(const TSharedPtr<FJsonObject>& Params);

	// Niagara VFX (Sprint 19)
	FString HandleSpawnNiagaraSystem(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetNiagaraParameter(const TSharedPtr<FJsonObject>& Params);
	FString HandleNiagaraControl(const TSharedPtr<FJsonObject>& Params);

	// Data Tables (Sprint 20)
	FString HandleCreateDataTable(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadDataTable(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddDataTableRow(const TSharedPtr<FJsonObject>& Params);

	// Sequencer / Cinematics (Sprint 18)
	FString HandleCreateLevelSequence(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadLevelSequence(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddSequenceTrack(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetSequencePlayback(const TSharedPtr<FJsonObject>& Params);

	// Material Graph Authoring (Sprint 17)
	FString HandleAddMaterialExpression(const TSharedPtr<FJsonObject>& Params);
	FString HandleConnectMaterialExpressions(const TSharedPtr<FJsonObject>& Params);
	FString HandleDeleteMaterialExpression(const TSharedPtr<FJsonObject>& Params);
	FString HandleRecompileMaterial(const TSharedPtr<FJsonObject>& Params);
	FString HandleListMaterialExpressions(const TSharedPtr<FJsonObject>& Params);

	// PIE + Editor Workflow (Sprint 16)
	FString HandlePlayInEditor(const TSharedPtr<FJsonObject>& Params);
	FString HandleExecuteConsoleCommand(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadLog(const TSharedPtr<FJsonObject>& Params);
	FString HandleGetEngineVersion(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetViewportCamera(const TSharedPtr<FJsonObject>& Params);
	FString HandleOpenAsset(const TSharedPtr<FJsonObject>& Params);
	FString HandleSaveAllAndClose(const TSharedPtr<FJsonObject>& Params);

	// Behavior Tree (Sprint 14)
	FString HandleCreateBehaviorTree(const TSharedPtr<FJsonObject>& Params);
	FString HandleCreateBlackboard(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddBlackboardKey(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadBehaviorTree(const TSharedPtr<FJsonObject>& Params);

	// Quality of Life (Sprint 15)
	FString HandleSearchAssets(const TSharedPtr<FJsonObject>& Params);
	FString HandleRenameAsset(const TSharedPtr<FJsonObject>& Params);
	FString HandleDeleteAsset(const TSharedPtr<FJsonObject>& Params);

	// Widget Blueprint / UMG (Sprint 13)
	FString HandleCreateWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);
	FString HandleAddWidget(const TSharedPtr<FJsonObject>& Params);
	FString HandleRemoveWidget(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetWidgetProperty(const TSharedPtr<FJsonObject>& Params);
	FString HandleReadWidgetTree(const TSharedPtr<FJsonObject>& Params);

	// Material System (Sprint 12)
	FString HandleCreateMaterial(const TSharedPtr<FJsonObject>& Params);
	FString HandleCreateMaterialInstance(const TSharedPtr<FJsonObject>& Params);
	FString HandleSetMaterialParameter(const TSharedPtr<FJsonObject>& Params);
	FString HandleListMaterialParameters(const TSharedPtr<FJsonObject>& Params);
	FString HandleAssignMaterialToActor(const TSharedPtr<FJsonObject>& Params);

	// Actor & Level Management (Sprint 11)
	FString HandleSpawnActor(const TSharedPtr<FJsonObject>& Params);
	FString HandleDestroyActor(const TSharedPtr<FJsonObject>& Params);
	FString HandleDuplicateActor(const TSharedPtr<FJsonObject>& Params);
	FString HandleGetCurrentLevel(const TSharedPtr<FJsonObject>& Params);
	FString HandleLoadLevel(const TSharedPtr<FJsonObject>& Params);
	FString HandleListLevels(const TSharedPtr<FJsonObject>& Params);

	// Helpers
	FString MakeResponse(bool bSuccess, const TSharedPtr<FJsonObject>& Data, const FString& Error = TEXT(""));
	FString MakeError(const FString& Error);
	class UBlueprint* LoadBlueprintFromPath(const FString& Path);

	FTcpListener* Listener = nullptr;
	bool bRunning = false;
	TArray<FSocket*> ClientSockets;
	FCriticalSection ClientSocketsLock;
};
