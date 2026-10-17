#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"

class FMCPServer;

class FClaudeUnrealMCPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void OnBlueprintCompiled();

	FMCPServer* Server = nullptr;
	FDelegateHandle OnBlueprintCompiledHandle;
	FTSTicker::FDelegateHandle TickerHandle;
};
