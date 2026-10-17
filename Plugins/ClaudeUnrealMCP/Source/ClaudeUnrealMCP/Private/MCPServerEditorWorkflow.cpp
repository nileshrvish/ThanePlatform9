#include "MCPServer.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Engine/World.h"
#include "LevelEditor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "HAL/PlatformFileManager.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "LevelEditorViewport.h"
#include "SLevelViewport.h"

// ===== PIE CONTROL =====

FString FMCPServer::HandlePlayInEditor(const TSharedPtr<FJsonObject>& Params)
{
	FString Op = TEXT("status");
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("operation"), Op);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

	if (Op == TEXT("start"))
	{
		if (GEditor->PlayWorld)
		{
			Data->SetBoolField(TEXT("already_playing"), true);
			return MakeResponse(true, Data);
		}

		// Request PIE start via deferred command (must run on game thread main loop)
		GEditor->RequestPlaySession(FRequestPlaySessionParams());
		Data->SetBoolField(TEXT("started"), true);
		return MakeResponse(true, Data);
	}
	else if (Op == TEXT("stop"))
	{
		if (!GEditor->PlayWorld)
		{
			Data->SetBoolField(TEXT("already_stopped"), true);
			return MakeResponse(true, Data);
		}

		GEditor->RequestEndPlayMap();
		Data->SetBoolField(TEXT("stopped"), true);
		return MakeResponse(true, Data);
	}
	else if (Op == TEXT("status"))
	{
		Data->SetBoolField(TEXT("is_playing"), GEditor->PlayWorld != nullptr);
		Data->SetBoolField(TEXT("is_simulating"), GEditor->bIsSimulatingInEditor);
		return MakeResponse(true, Data);
	}

	return MakeError(FString::Printf(TEXT("Unknown operation: %s (use start, stop, or status)"), *Op));
}

// ===== CONSOLE COMMAND =====

FString FMCPServer::HandleExecuteConsoleCommand(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString Command;
	if (!Params->TryGetStringField(TEXT("command"), Command) || Command.IsEmpty())
	{
		return MakeError(TEXT("command parameter is required (e.g. 'stat fps', 'ShowFlag.Collision 1')"));
	}

	UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return MakeError(TEXT("No world context available"));
	}

	// Special: "setrot <pitch> <yaw>" directly sets PlayerController control rotation in PIE
	if (Command.StartsWith(TEXT("setrot ")))
	{
		TArray<FString> Parts;
		Command.ParseIntoArrayWS(Parts);
		if (Parts.Num() >= 3 && World->GetFirstPlayerController())
		{
			float Pitch = FCString::Atof(*Parts[1]);
			float Yaw = FCString::Atof(*Parts[2]);
			World->GetFirstPlayerController()->SetControlRotation(FRotator(Pitch, Yaw, 0));

			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetStringField(TEXT("command"), Command);
			Data->SetNumberField(TEXT("pitch"), Pitch);
			Data->SetNumberField(TEXT("yaw"), Yaw);
			return MakeResponse(true, Data);
		}
		return MakeError(TEXT("setrot requires: setrot <pitch> <yaw>. PIE must be running."));
	}

	// Execute the console command
	GEngine->Exec(World, *Command);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("command"), Command);
	Data->SetBoolField(TEXT("executed"), true);
	Data->SetStringField(TEXT("note"), TEXT("Output not captured — use read_log with keyword filter to see results"));
	return MakeResponse(true, Data);
}

// ===== LOG READING =====
// Reference: GenOrca reads FPaths::ProjectLogDir() + glob for latest .log

FString FMCPServer::HandleReadLog(const TSharedPtr<FJsonObject>& Params)
{
	int32 LineCount = 50;
	FString Keyword;

	if (Params.IsValid())
	{
		Params->TryGetNumberField(TEXT("line_count"), LineCount);
		Params->TryGetStringField(TEXT("keyword"), Keyword);
	}

	if (LineCount <= 0) LineCount = 50;
	if (LineCount > 5000) LineCount = 5000;

	// Find the latest log file
	FString LogDir = FPaths::ProjectLogDir();
	TArray<FString> LogFiles;
	IFileManager::Get().FindFiles(LogFiles, *FPaths::Combine(LogDir, TEXT("*.log")), true, false);

	if (LogFiles.Num() == 0)
	{
		return MakeError(TEXT("No log files found in project log directory"));
	}

	// Find the most recently modified log file
	FString LatestLog;
	FDateTime LatestTime = FDateTime::MinValue();
	for (const FString& LogFile : LogFiles)
	{
		FString FullPath = FPaths::Combine(LogDir, LogFile);
		FDateTime ModTime = IFileManager::Get().GetTimeStamp(*FullPath);
		if (ModTime > LatestTime)
		{
			LatestTime = ModTime;
			LatestLog = FullPath;
		}
	}

	// Read the file
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *LatestLog))
	{
		return MakeError(FString::Printf(TEXT("Failed to read log file: %s"), *LatestLog));
	}

	// Split into lines
	TArray<FString> AllLines;
	FileContent.ParseIntoArrayLines(AllLines);

	// Filter by keyword if provided
	TArray<FString> FilteredLines;
	if (!Keyword.IsEmpty())
	{
		for (const FString& Line : AllLines)
		{
			if (Line.Contains(Keyword, ESearchCase::IgnoreCase))
			{
				FilteredLines.Add(Line);
			}
		}
	}
	else
	{
		FilteredLines = AllLines;
	}

	// Take last N lines
	int32 StartIdx = FMath::Max(0, FilteredLines.Num() - LineCount);
	FString Output;
	int32 ReturnedCount = 0;
	for (int32 i = StartIdx; i < FilteredLines.Num(); ++i)
	{
		Output += FilteredLines[i] + TEXT("\n");
		ReturnedCount++;
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("log_file"), FPaths::GetCleanFilename(LatestLog));
	Data->SetNumberField(TEXT("total_lines"), AllLines.Num());
	Data->SetNumberField(TEXT("returned_lines"), ReturnedCount);
	if (!Keyword.IsEmpty())
	{
		Data->SetStringField(TEXT("keyword"), Keyword);
		Data->SetNumberField(TEXT("matched_lines"), FilteredLines.Num());
	}
	Data->SetStringField(TEXT("log"), Output);
	return MakeResponse(true, Data);
}

// ===== ENGINE VERSION =====

FString FMCPServer::HandleGetEngineVersion(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("version"), FEngineVersion::Current().ToString());
	Data->SetNumberField(TEXT("major"), FEngineVersion::Current().GetMajor());
	Data->SetNumberField(TEXT("minor"), FEngineVersion::Current().GetMinor());
	Data->SetNumberField(TEXT("patch"), FEngineVersion::Current().GetPatch());
	Data->SetStringField(TEXT("branch"), FApp::GetBranchName());
	Data->SetStringField(TEXT("build_config"),
#if UE_BUILD_DEBUG
		TEXT("Debug")
#elif UE_BUILD_DEVELOPMENT
		TEXT("Development")
#elif UE_BUILD_SHIPPING
		TEXT("Shipping")
#else
		TEXT("Unknown")
#endif
	);
	return MakeResponse(true, Data);
}

// ===== VIEWPORT CAMERA =====

FString FMCPServer::HandleSetViewportCamera(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString Op = TEXT("get");
	Params->TryGetStringField(TEXT("operation"), Op);

	// Get the active level editor viewport
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<SLevelViewport> ActiveViewport = LevelEditor.GetFirstActiveLevelViewport();
	if (!ActiveViewport.IsValid())
	{
		return MakeError(TEXT("No active level viewport"));
	}

	FLevelEditorViewportClient& ViewportClient = ActiveViewport->GetLevelViewportClient();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();

	if (Op == TEXT("get"))
	{
		FVector Loc = ViewportClient.GetViewLocation();
		FRotator Rot = ViewportClient.GetViewRotation();
		Data->SetStringField(TEXT("location"), FString::Printf(TEXT("(%.1f, %.1f, %.1f)"), Loc.X, Loc.Y, Loc.Z));
		Data->SetStringField(TEXT("rotation"), FString::Printf(TEXT("(P=%.1f, Y=%.1f, R=%.1f)"), Rot.Pitch, Rot.Yaw, Rot.Roll));
		Data->SetNumberField(TEXT("x"), Loc.X);
		Data->SetNumberField(TEXT("y"), Loc.Y);
		Data->SetNumberField(TEXT("z"), Loc.Z);
		Data->SetNumberField(TEXT("pitch"), Rot.Pitch);
		Data->SetNumberField(TEXT("yaw"), Rot.Yaw);
		Data->SetNumberField(TEXT("roll"), Rot.Roll);
	}
	else if (Op == TEXT("set"))
	{
		FVector Loc = ViewportClient.GetViewLocation();
		FRotator Rot = ViewportClient.GetViewRotation();

		if (Params->HasField(TEXT("x"))) Loc.X = Params->GetNumberField(TEXT("x"));
		if (Params->HasField(TEXT("y"))) Loc.Y = Params->GetNumberField(TEXT("y"));
		if (Params->HasField(TEXT("z"))) Loc.Z = Params->GetNumberField(TEXT("z"));
		if (Params->HasField(TEXT("pitch"))) Rot.Pitch = Params->GetNumberField(TEXT("pitch"));
		if (Params->HasField(TEXT("yaw"))) Rot.Yaw = Params->GetNumberField(TEXT("yaw"));
		if (Params->HasField(TEXT("roll"))) Rot.Roll = Params->GetNumberField(TEXT("roll"));

		ViewportClient.SetViewLocation(Loc);
		ViewportClient.SetViewRotation(Rot);
		ViewportClient.Invalidate();

		Data->SetStringField(TEXT("location"), FString::Printf(TEXT("(%.1f, %.1f, %.1f)"), Loc.X, Loc.Y, Loc.Z));
		Data->SetStringField(TEXT("rotation"), FString::Printf(TEXT("(P=%.1f, Y=%.1f, R=%.1f)"), Rot.Pitch, Rot.Yaw, Rot.Roll));
	}
	else
	{
		return MakeError(FString::Printf(TEXT("Unknown operation: %s (use get or set)"), *Op));
	}

	return MakeResponse(true, Data);
}

// ===== OPEN ASSET IN EDITOR =====

FString FMCPServer::HandleOpenAsset(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params.IsValid()) return MakeError(TEXT("Missing params"));

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("path"), AssetPath) || AssetPath.IsEmpty())
	{
		return MakeError(TEXT("path parameter is required (e.g. '/Game/Blueprints/SandboxCharacter_CMC')"));
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		return MakeError(TEXT("AssetEditorSubsystem not available"));
	}

	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	if (!Asset)
	{
		return MakeError(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
	}

	bool bOpened = AssetEditorSubsystem->OpenEditorForAsset(Asset);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), AssetPath);
	Data->SetStringField(TEXT("name"), Asset->GetName());
	Data->SetBoolField(TEXT("opened"), bOpened);
	return MakeResponse(bOpened, Data);
}
