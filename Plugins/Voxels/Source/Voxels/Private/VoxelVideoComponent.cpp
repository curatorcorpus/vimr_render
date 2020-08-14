


// Fill out your copyright notice in the Description page of Project Settings
#include "VoxelVideoComponent.h"
#include "VoxelRenderSubComponent.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Editor/MainFrame/Public/Interfaces/IMainFrameModule.h"
#include "Runtime/SlateCore/Public/Widgets/SWindow.h"
#include "Paths.h"

#include "HAL/FileManager.h"

#include <chrono>

using namespace std::placeholders;
using std::string;

DEFINE_LOG_CATEGORY(VoxVidLog);

// Sets default values for this component's properties
UVoxelVideoComponent::UVoxelVideoComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	//OnPlaybackFinished.AddDynamic(this, &UVoxelVideoComponent::OnPlaybackFinishedEvent);
}

// Called when the game starts
void UVoxelVideoComponent::BeginPlay()
{
	Super::BeginPlay();

	char* datapath;
	size_t dplen;

	voxelvideosPath = FPaths::ProjectContentDir() + FString("VoxelVideos/");

	if(VIMRconfig->GetString("SharedDataPath", &datapath, dplen))
	{

	}
	else{
		//Exit game?
	}
}

// Called every frame
void UVoxelVideoComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (VoxelVideoReader != nullptr) {
		if (VoxelVideoReader->State() == VIMR::VoxVidPlayer::PlayState::Finished) {
			OnPlaybackFinished.Broadcast();
			_stop();
			UE_LOG(VoxVidLog, Log, TEXT("Playback Finished"));
			VoxelVideoReader->Close();
		}
		else if(VoxelVideoReader->State() == VIMR::VoxVidPlayer::PlayState::Paused) {
			UE_LOG(VoxVidLog, Log, TEXT("Playback is Paused"));
		}
	}

	if (!audioVoxStack.empty()) {
		AudioStreams.begin()->second->SetWorldTransform(FTransform(audioVoxStack.top()));
		audioVoxStack.pop();
	}
	else if (VoxelVideoReader->State() == VIMR::VoxVidPlayer::PlayState::Paused) {
		//UE_LOG(VoxVidLog, Log, TEXT("Playback is Paused"));
	}

	if (!cmdStack.empty()) {
		cmdStack.top()();
		cmdStack.pop();
	}
}

void UVoxelVideoComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	VoxelVideoReader->Close();
}

void UVoxelVideoComponent::_pause()
{
	PlaybackFinished = false;
	VoxelVideoReader->Pause();
	IsPaused = true;
	for (auto i : AudioStreams) {
		i.second->Pause();
	}
}

void UVoxelVideoComponent::_play()
{
	IsPaused = false;
	PlaybackFinished = false;
	for (auto i : AudioStreams) {
		i.second->Start();
	}
	VoxelVideoReader->Play();
}

void UVoxelVideoComponent::_stop()
{
	_pause();
	_restart();
	_pause();
}

void UVoxelVideoComponent::_restart()
{
	PlaybackFinished = false;
	VoxelVideoReader->Restart();
	for (auto i : AudioStreams) {
		i.second->Stop();
		i.second->Start();
	}
}

void UVoxelVideoComponent::_setLoop(bool isLooping)
{
	if (VoxelVideoReader != NULL)
	{
		VoxelVideoReader->Loop(isLooping);
	}
}

void UVoxelVideoComponent::LoadVoxelVideo(FString filepath)
{
	if (VoxelVideoReader != nullptr)
	{
		VoxelVideoReader->Close();
	}

	FString file_path = voxelvideosPath + RecordingPath;

	VoxelVideoReader = new VIMR::VoxVidPlayer(TCHAR_TO_ANSI(*file_path), std::bind(&UVoxelVideoComponent::CopyVoxelData, this, std::placeholders::_1));
	VoxelVideoReader->Loop(true);
	UE_LOG(VoxVidLog, Log, TEXT("Loaded file %s"), *file_path);

	VIMR::AudioStream tmp_astrm;
	while (VoxelVideoReader->GetNextAudioStream(tmp_astrm))
	{
		URuntimeAudioSource* newSource = NewObject<URuntimeAudioSource>(this);
		FString wav_path = voxelvideosPath + FString(tmp_astrm.file_name);
		FString wav_label = FString(tmp_astrm.voxel_label);

		newSource->RegisterComponent();
		newSource->AttachToComponent(this, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
		newSource->LoadWav(wav_path);
		AudioStreams[tmp_astrm.voxel_label] = newSource;

		UE_LOG(VoxLog, Log, TEXT("Loaded wav: %s"), *wav_path);
		UE_LOG(VoxLog, Log, TEXT("Loaded wav: %s"), *wav_label);
	}
}

TArray<FString> UVoxelVideoComponent::GetAllRecordings()
{
	TArray<FString> files;
	files.Empty();

	FString recordingPath = FPaths::ProjectContentDir() + "VoxelVideos/";
	FString voxelvideo_ext = "vx3";

	if (FPaths::DirectoryExists(*recordingPath))
	{
		IFileManager::Get().FindFiles(files, *recordingPath, *voxelvideo_ext);

		for (int i = 0; i < files.Num(); i++)
		{
			UE_LOG(LogTemp, Log, TEXT("These files Exists: %s"), *files[i]);
		}

		UE_LOG(LogTemp, Log, TEXT("This Path Exists: %s"), *recordingPath)
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("This path doesn't exist"));
	}

	return files;
}

void UVoxelVideoComponent::Pause() { cmdStack.push((PlaybackControlFnPtr)std::bind(&UVoxelVideoComponent::_pause, this)); }
void UVoxelVideoComponent::Play() { cmdStack.push((PlaybackControlFnPtr)std::bind(&UVoxelVideoComponent::_play, this)); }
void UVoxelVideoComponent::Stop() { cmdStack.push((PlaybackControlFnPtr)std::bind(&UVoxelVideoComponent::_stop, this)); }
void UVoxelVideoComponent::Restart() { cmdStack.push((PlaybackControlFnPtr)std::bind(&UVoxelVideoComponent::_restart, this)); }