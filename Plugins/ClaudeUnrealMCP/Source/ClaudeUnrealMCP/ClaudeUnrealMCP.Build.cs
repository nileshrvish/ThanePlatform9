using UnrealBuildTool;

public class ClaudeUnrealMCP : ModuleRules
{
	public ClaudeUnrealMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Sockets",
			"Networking"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"BlueprintEditorLibrary",
			"BlueprintGraph",
			"Kismet",
			"AssetRegistry",
			"Json",
			"JsonUtilities",
			"EnhancedInput",
			"InputCore",
			"UMGEditor",  // For UWidgetBlueprint
			"Chooser",    // For UChooserTable migration
			"StructUtils",  // For FInstancedStruct
			"EditorScriptingUtilities",  // For UEditorAssetLibrary
			"AssetTools",  // For material/asset creation factories
			"UMG",  // For UWidget, UWidgetTree, widget components
			"AIModule",  // For BehaviorTree, BlackboardData
			"BehaviorTreeEditor",  // For BT/BB factories
			"LevelEditor",  // For viewport camera control
			"Slate",  // For SLevelViewport
			"SlateCore",
			"MaterialEditor",  // For UMaterialEditingLibrary
			"Niagara",  // For UNiagaraComponent, UNiagaraSystem
			"GameplayTags",  // For UGameplayTagsManager
			"StateTreeModule",  // For UStateTree
			"StateTreeEditorModule",  // For UStateTreeFactory
			"AudioEditor",  // For SoundCue factory
			"Landscape",  // For ALandscapeProxy
			"ProceduralMeshComponent",  // For UProceduralMeshComponent
			"NavigationSystem",  // For ANavMeshBoundsVolume
			"LevelSequence",  // For ULevelSequence
			"MovieScene",  // For UMovieScene, tracks, sections
			"MovieSceneTracks",  // For transform/audio/event tracks
			"AnimGraph",  // For UAnimGraphNode_ModifyBone, AnimGraph editor nodes
			"AnimGraphRuntime",  // For FAnimNode_ModifyBone runtime structs
			"SkeletonEditor",  // For IEditableSkeleton::AddSocket/RenameSocket
			"AnimationBlueprintLibrary",  // For UAnimPoseExtensions::GetAnimPoseAtTime/GetBonePose
			"RenderCore"  // For FlushRenderingCommands (synchronous screenshot capture)
		});
	}
}
