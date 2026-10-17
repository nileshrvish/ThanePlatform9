#include "ClaudeUnrealMCPModule.h"
#include "MCPServer.h"
#include "Editor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "FClaudeUnrealMCPModule"

void FClaudeUnrealMCPModule::StartupModule()
{
	Server = new FMCPServer();
	if (Server->Start(9877))
	{
		UE_LOG(LogTemp, Log, TEXT("ClaudeUnrealMCP: Server started on port 9877"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ClaudeUnrealMCP: Failed to start server"));
	}

	// Use a ticker to defer registration until GEditor is available
	// This is more reliable than OnPostEngineInit which may fire before editor modules load
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this](float DeltaTime) -> bool
		{
			if (GEditor)
			{
				OnBlueprintCompiledHandle = GEditor->OnBlueprintCompiled().AddRaw(this, &FClaudeUnrealMCPModule::OnBlueprintCompiled);
				UE_LOG(LogTemp, Log, TEXT("ClaudeUnrealMCP: Registered blueprint compile callback"));
				return false; // Stop ticking
			}
			return true; // Keep ticking until GEditor is available
		}),
		0.1f // Check every 100ms
	);
}

void FClaudeUnrealMCPModule::ShutdownModule()
{
	// Remove ticker if still active
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}

	// Unregister blueprint compilation callback
	if (GEditor && OnBlueprintCompiledHandle.IsValid())
	{
		GEditor->OnBlueprintCompiled().Remove(OnBlueprintCompiledHandle);
	}

	if (Server)
	{
		Server->Stop();
		delete Server;
		Server = nullptr;
	}
}

void FClaudeUnrealMCPModule::OnBlueprintCompiled()
{
	// Defer reconstruction with a small delay to let blueprint compilation and reinstancing fully complete
	// The callback fires during compilation, before all objects are ready
	if (Server)
	{
		// Server started successfully
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FClaudeUnrealMCPModule, ClaudeUnrealMCP)
