// Sprint 18 — Sequencer / Cinematics
#include "MCPServer.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSection.h"
#include "MovieSceneBinding.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Tracks/MovieSceneFadeTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneBoolTrack.h"
#include "EditorAssetLibrary.h"
#include "ScopedTransaction.h"

FString FMCPServer::HandleCreateLevelSequence(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath, AssetName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return MakeError(TEXT("asset_path required (e.g. '/Game/Cinematics')"));
	if (!Params->TryGetStringField(TEXT("asset_name"), AssetName))
		return MakeError(TEXT("asset_name required"));

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Create the LevelSequence via AssetTools
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, AssetPath, ULevelSequence::StaticClass(), nullptr);
	if (!NewAsset)
		return MakeError(TEXT("Failed to create level sequence"));

	ULevelSequence* Sequence = Cast<ULevelSequence>(NewAsset);

	// Set playback range if provided
	if (Sequence && Sequence->GetMovieScene())
	{
		double StartSeconds = 0.0, EndSeconds = 5.0;
		Params->TryGetNumberField(TEXT("start_time"), StartSeconds);
		Params->TryGetNumberField(TEXT("end_time"), EndSeconds);

		UMovieScene* MovieScene = Sequence->GetMovieScene();
		FFrameRate TickResolution = MovieScene->GetTickResolution();
		MovieScene->SetPlaybackRange(
			FFrameNumber(FMath::RoundToInt32(StartSeconds * TickResolution.AsDecimal())),
			FMath::RoundToInt32((EndSeconds - StartSeconds) * TickResolution.AsDecimal()));
	}

	UEditorAssetLibrary::SaveAsset(NewAsset->GetPathName(), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), NewAsset->GetPathName());
	Data->SetStringField(TEXT("name"), AssetName);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleReadLevelSequence(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString SequencePath;
	if (!Params->TryGetStringField(TEXT("path"), SequencePath))
		return MakeError(TEXT("path required"));

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
	if (!Sequence)
		return MakeError(FString::Printf(TEXT("Level sequence not found: %s"), *SequencePath));

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
		return MakeError(TEXT("MovieScene is null"));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), SequencePath);

	// Playback range
	FFrameRate TickRes = MovieScene->GetTickResolution();
	TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
	Data->SetNumberField(TEXT("start_frame"), PlaybackRange.GetLowerBoundValue().Value);
	Data->SetNumberField(TEXT("end_frame"), PlaybackRange.GetUpperBoundValue().Value);
	Data->SetNumberField(TEXT("start_seconds"), PlaybackRange.GetLowerBoundValue().Value / TickRes.AsDecimal());
	Data->SetNumberField(TEXT("end_seconds"), PlaybackRange.GetUpperBoundValue().Value / TickRes.AsDecimal());
	Data->SetNumberField(TEXT("tick_resolution"), TickRes.AsDecimal());

	// Master tracks (not bound to any object)
	TArray<TSharedPtr<FJsonValue>> MasterTracks;
	for (UMovieSceneTrack* Track : MovieScene->GetTracks())
	{
		if (!Track) continue;
		TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
		TrackObj->SetStringField(TEXT("name"), Track->GetDisplayName().ToString());
		TrackObj->SetStringField(TEXT("class"), Track->GetClass()->GetName());
		TrackObj->SetNumberField(TEXT("section_count"), Track->GetAllSections().Num());
		MasterTracks.Add(MakeShared<FJsonValueObject>(TrackObj));
	}
	Data->SetArrayField(TEXT("master_tracks"), MasterTracks);

	// Bindings (object-bound tracks)
	TArray<TSharedPtr<FJsonValue>> Bindings;
	for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
	{
		TSharedPtr<FJsonObject> BindObj = MakeShared<FJsonObject>();
		BindObj->SetStringField(TEXT("name"), Binding.GetName());
		BindObj->SetStringField(TEXT("guid"), Binding.GetObjectGuid().ToString());

		TArray<TSharedPtr<FJsonValue>> BoundTracks;
		for (UMovieSceneTrack* Track : Binding.GetTracks())
		{
			if (!Track) continue;
			TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
			TrackObj->SetStringField(TEXT("name"), Track->GetDisplayName().ToString());
			TrackObj->SetStringField(TEXT("class"), Track->GetClass()->GetName());
			TrackObj->SetNumberField(TEXT("section_count"), Track->GetAllSections().Num());
			BoundTracks.Add(MakeShared<FJsonValueObject>(TrackObj));
		}
		BindObj->SetArrayField(TEXT("tracks"), BoundTracks);
		Bindings.Add(MakeShared<FJsonValueObject>(BindObj));
	}
	Data->SetArrayField(TEXT("bindings"), Bindings);

	return MakeResponse(true, Data);
}

FString FMCPServer::HandleAddSequenceTrack(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString SequencePath, TrackType;
	if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath))
		return MakeError(TEXT("sequence_path required"));
	if (!Params->TryGetStringField(TEXT("track_type"), TrackType))
		return MakeError(TEXT("track_type required (Transform, SkeletalAnimation, Audio, Event, Fade, CameraCut, Float, Bool)"));

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
	if (!Sequence)
		return MakeError(FString::Printf(TEXT("Level sequence not found: %s"), *SequencePath));

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
		return MakeError(TEXT("MovieScene is null"));

	// Resolve track class
	static const TMap<FString, UClass*> TrackClasses = {
		{TEXT("Transform"), UMovieScene3DTransformTrack::StaticClass()},
		{TEXT("SkeletalAnimation"), UMovieSceneSkeletalAnimationTrack::StaticClass()},
		{TEXT("Audio"), UMovieSceneAudioTrack::StaticClass()},
		{TEXT("Event"), UMovieSceneEventTrack::StaticClass()},
		{TEXT("Fade"), UMovieSceneFadeTrack::StaticClass()},
		{TEXT("CameraCut"), UMovieSceneCameraCutTrack::StaticClass()},
		{TEXT("Float"), UMovieSceneFloatTrack::StaticClass()},
		{TEXT("Bool"), UMovieSceneBoolTrack::StaticClass()},
	};

	const UClass* const* TrackClass = TrackClasses.Find(TrackType);
	if (!TrackClass)
		return MakeError(FString::Printf(TEXT("Unknown track type: %s"), *TrackType));

	FScopedTransaction Transaction(FText::FromString(TEXT("MCP: Add Sequence Track")));

	// Check if this is a master track or bound to an object
	FString BindingGuid;
	UMovieSceneTrack* NewTrack = nullptr;

	if (Params->TryGetStringField(TEXT("binding_guid"), BindingGuid) && !BindingGuid.IsEmpty())
	{
		FGuid Guid;
		if (!FGuid::Parse(BindingGuid, Guid))
			return MakeError(TEXT("Invalid binding_guid format"));
		NewTrack = MovieScene->AddTrack(const_cast<UClass*>(*TrackClass), Guid);
	}
	else
	{
		// Master track (not bound to any object)
		NewTrack = MovieScene->AddTrack(const_cast<UClass*>(*TrackClass));
	}

	if (!NewTrack)
		return MakeError(TEXT("Failed to add track"));

	// Add a default section
	UMovieSceneSection* Section = NewTrack->CreateNewSection();
	if (Section)
	{
		Section->SetRange(MovieScene->GetPlaybackRange());
		NewTrack->AddSection(*Section);
	}

	UEditorAssetLibrary::SaveAsset(SequencePath, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("track_type"), TrackType);
	Data->SetStringField(TEXT("track_name"), NewTrack->GetDisplayName().ToString());
	Data->SetBoolField(TEXT("has_section"), Section != nullptr);
	return MakeResponse(true, Data);
}

FString FMCPServer::HandleSetSequencePlayback(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString SequencePath;
	if (!Params->TryGetStringField(TEXT("sequence_path"), SequencePath))
		return MakeError(TEXT("sequence_path required"));

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
	if (!Sequence)
		return MakeError(FString::Printf(TEXT("Level sequence not found: %s"), *SequencePath));

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
		return MakeError(TEXT("MovieScene is null"));

	FFrameRate TickRes = MovieScene->GetTickResolution();

	double StartSeconds = -1, EndSeconds = -1;
	Params->TryGetNumberField(TEXT("start_time"), StartSeconds);
	Params->TryGetNumberField(TEXT("end_time"), EndSeconds);

	if (StartSeconds >= 0 && EndSeconds > StartSeconds)
	{
		MovieScene->SetPlaybackRange(
			FFrameNumber(FMath::RoundToInt32(StartSeconds * TickRes.AsDecimal())),
			FMath::RoundToInt32((EndSeconds - StartSeconds) * TickRes.AsDecimal()));
	}

	UEditorAssetLibrary::SaveAsset(SequencePath, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
	Data->SetNumberField(TEXT("start_seconds"), Range.GetLowerBoundValue().Value / TickRes.AsDecimal());
	Data->SetNumberField(TEXT("end_seconds"), Range.GetUpperBoundValue().Value / TickRes.AsDecimal());
	return MakeResponse(true, Data);
}
