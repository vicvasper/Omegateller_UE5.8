// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"

/**
 * Manages the bundled local LLM backend (llama.cpp server) that gives the
 * narrative editor real language understanding without any cloud dependency.
 *
 * - Spawns ThirdParty/LlamaServer/llama-server.exe with the bundled GGUF model
 *   the first time the OmegaTeller tab opens, and kills it with the editor.
 * - Reads Config/OmegaTellerConfig.ini ([OmegaAI] section) so users can point
 *   the plugin at any OpenAI-compatible endpoint instead (Ollama, LM Studio,
 *   a remote API...) or add optional cloud API keys.
 * - Produces the JSON blob injected into the web editor's AI_CONFIG.
 */
class FOmegaLocalAI
{
public:
	static FOmegaLocalAI& Get();

	/** Spawn the bundled server if configured, not already running, and not externally provided. */
	void EnsureServerRunning();

	/** Terminate the child server process (called from module shutdown). */
	void ShutdownServer();

	/** True when the bundled exe and a model file exist on disk. */
	bool IsBundledServerAvailable() const;

	/** Endpoint the editor should call (bundled server or user override). */
	FString GetEndpoint() const { return Endpoint; }

	/** JSON string (object) with the AI configuration for the web editor. */
	FString BuildJSConfigJSON() const;

private:
	FOmegaLocalAI();
	~FOmegaLocalAI();

	void LoadConfig();
	FString FindBundledModel() const;
	FString GetLlamaServerPath() const;

	// Config ([OmegaAI] in OmegaTellerConfig.ini)
	bool bAutoStartLocalServer = true;
	int32 LocalPort = 17811;
	FString Endpoint;      // full chat/completions URL
	FString ModelName;     // model id sent in requests (informative for llama.cpp)
	FString ModelFileOverride;
	int32 ContextSize = 4096;
	FString GeminiKey;
	FString GroqKey;
	FString GeminiModel;
	FString GroqModel;

	// Runtime state
	FProcHandle ServerHandle;
	bool bConfigLoaded = false;
};
