// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmegaLocalAI.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FOmegaLocalAI& FOmegaLocalAI::Get()
{
	static FOmegaLocalAI Instance;
	return Instance;
}

FOmegaLocalAI::FOmegaLocalAI()
{
	LoadConfig();
}

FOmegaLocalAI::~FOmegaLocalAI()
{
	ShutdownServer();
}

static FString GetPluginBaseDir()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("OmegaTellerPlugin"));
	return Plugin.IsValid() ? Plugin->GetBaseDir() : FString();
}

void FOmegaLocalAI::LoadConfig()
{
	if (bConfigLoaded)
	{
		return;
	}
	bConfigLoaded = true;

	const FString BaseDir = GetPluginBaseDir();
	const FString IniPath = FPaths::ConvertRelativePathToFull(BaseDir / TEXT("Config/OmegaTellerConfig.ini"));

	FConfigFile Config;
	if (FPaths::FileExists(IniPath))
	{
		Config.Read(IniPath);
	}

	const TCHAR* Section = TEXT("OmegaAI");
	Config.GetBool(Section, TEXT("bAutoStartLocalServer"), bAutoStartLocalServer);
	Config.GetInt(Section, TEXT("LocalPort"), LocalPort);
	Config.GetString(Section, TEXT("LocalEndpoint"), Endpoint);
	Config.GetString(Section, TEXT("LocalModel"), ModelName);
	Config.GetString(Section, TEXT("ModelFile"), ModelFileOverride);
	Config.GetInt(Section, TEXT("ContextSize"), ContextSize);
	Config.GetString(Section, TEXT("GeminiKey"), GeminiKey);
	Config.GetString(Section, TEXT("GroqKey"), GroqKey);
	Config.GetString(Section, TEXT("GeminiModel"), GeminiModel);
	Config.GetString(Section, TEXT("GroqModel"), GroqModel);

	if (Endpoint.IsEmpty())
	{
		Endpoint = FString::Printf(TEXT("http://127.0.0.1:%d/v1/chat/completions"), LocalPort);
	}
	if (ModelName.IsEmpty())
	{
		ModelName = TEXT("local");
	}

	UE_LOG(LogTemp, Log, TEXT("[OmegaAI] Config: endpoint=%s autoStart=%s"),
		*Endpoint, bAutoStartLocalServer ? TEXT("yes") : TEXT("no"));
}

FString FOmegaLocalAI::GetLlamaServerPath() const
{
	return FPaths::ConvertRelativePathToFull(
		GetPluginBaseDir() / TEXT("ThirdParty/LlamaServer/llama-server.exe"));
}

FString FOmegaLocalAI::FindBundledModel() const
{
	const FString ModelsDir = FPaths::ConvertRelativePathToFull(GetPluginBaseDir() / TEXT("ThirdParty/Models"));

	if (!ModelFileOverride.IsEmpty())
	{
		const FString Explicit = FPaths::IsRelative(ModelFileOverride)
			? ModelsDir / ModelFileOverride
			: ModelFileOverride;
		return FPaths::FileExists(Explicit) ? Explicit : FString();
	}

	TArray<FString> Found;
	IFileManager::Get().FindFiles(Found, *(ModelsDir / TEXT("*.gguf")), true, false);
	return Found.Num() > 0 ? ModelsDir / Found[0] : FString();
}

bool FOmegaLocalAI::IsBundledServerAvailable() const
{
	return FPaths::FileExists(GetLlamaServerPath()) && !FindBundledModel().IsEmpty();
}

void FOmegaLocalAI::EnsureServerRunning()
{
	if (!bAutoStartLocalServer)
	{
		return;
	}

	// Only manage the bundled server for localhost endpoints; a custom remote
	// endpoint means the user runs their own backend
	if (!Endpoint.Contains(TEXT("127.0.0.1")) && !Endpoint.Contains(TEXT("localhost")))
	{
		return;
	}

	if (ServerHandle.IsValid() && FPlatformProcess::IsProcRunning(ServerHandle))
	{
		return;
	}

	const FString ServerExe = GetLlamaServerPath();
	const FString ModelPath = FindBundledModel();
	if (!FPaths::FileExists(ServerExe) || ModelPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OmegaAI] Bundled local AI not found (exe: %s, model dir: ThirdParty/Models). "
			     "The editor will fall back to remote APIs or heuristics."), *ServerExe);
		return;
	}

	const int32 Threads = FMath::Clamp(FPlatformMisc::NumberOfCoresIncludingHyperthreads() - 2, 2, 16);
	const FString Args = FString::Printf(
		TEXT("-m \"%s\" --host 127.0.0.1 --port %d --ctx-size %d -t %d --log-disable"),
		*ModelPath, LocalPort, ContextSize, Threads);

	UE_LOG(LogTemp, Log, TEXT("[OmegaAI] Starting local LLM server: llama-server %s"), *Args);

	ServerHandle = FPlatformProcess::CreateProc(
		*ServerExe, *Args,
		/*bLaunchDetached*/ false,
		/*bLaunchHidden*/ true,
		/*bLaunchReallyHidden*/ true,
		/*OutProcessID*/ nullptr,
		/*PriorityModifier*/ -1,
		/*OptionalWorkingDirectory*/ *FPaths::GetPath(ServerExe),
		/*PipeWriteChild*/ nullptr);

	if (!ServerHandle.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[OmegaAI] Failed to start llama-server"));
	}
}

void FOmegaLocalAI::ShutdownServer()
{
	if (ServerHandle.IsValid())
	{
		if (FPlatformProcess::IsProcRunning(ServerHandle))
		{
			FPlatformProcess::TerminateProc(ServerHandle, /*KillTree*/ true);
		}
		FPlatformProcess::CloseProc(ServerHandle);
		ServerHandle.Reset();
		UE_LOG(LogTemp, Log, TEXT("[OmegaAI] Local LLM server stopped"));
	}
}

FString FOmegaLocalAI::BuildJSConfigJSON() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

	// Only advertise the local endpoint when something can actually answer:
	// the bundled server, or a custom endpoint the user configured explicitly
	const bool bLocalUsable = IsBundledServerAvailable()
		|| !Endpoint.Contains(TEXT("127.0.0.1")) || !bAutoStartLocalServer;
	Root->SetStringField(TEXT("localEndpoint"), bLocalUsable ? Endpoint : FString());
	Root->SetStringField(TEXT("localModel"), ModelName);
	Root->SetStringField(TEXT("geminiKey"), GeminiKey);
	Root->SetStringField(TEXT("groqKey"), GroqKey);
	if (!GeminiModel.IsEmpty())
	{
		Root->SetStringField(TEXT("geminiModel"), GeminiModel);
	}
	if (!GroqModel.IsEmpty())
	{
		Root->SetStringField(TEXT("groqModel"), GroqModel);
	}

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root, Writer);
	return Output;
}
