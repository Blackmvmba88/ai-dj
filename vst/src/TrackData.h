/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (C) 2025 Anthony Charretier
 */

#pragma once
#include <JuceHeader.h>
#include "DjIaClient.h"

struct TrackPage
{
	juce::AudioSampleBuffer audioBuffer;
	juce::String audioFilePath;
	int numSamples = 0;
	double sampleRate = 48000.0;
	float originalBpm = 126.0f;

	juce::String prompt;
	juce::String selectedPrompt;
	juce::String generationPrompt;
	float generationBpm = 126.0f;
	juce::String generationKey;
	int generationDuration = 6;
	std::vector<juce::String> preferredStems = {};
	juce::String stems;

	double loopStart = 0.0;
	double loopEnd = 4.0;
	std::atomic<bool> useOriginalFile{ false };
	std::atomic<bool> hasOriginalVersion{ false };
	juce::AudioBuffer<float> originalStagingBuffer;

	std::atomic<bool> isLoaded{ false };
	std::atomic<bool> isLoading{ false };

	TrackPage() = default;

	TrackPage(const TrackPage& other) {
		audioBuffer = other.audioBuffer;
		audioFilePath = other.audioFilePath;
		numSamples = other.numSamples;
		sampleRate = other.sampleRate;
		originalBpm = other.originalBpm;
		prompt = other.prompt;
		selectedPrompt = other.selectedPrompt;
		generationPrompt = other.generationPrompt;
		generationBpm = other.generationBpm;
		generationKey = other.generationKey;
		generationDuration = other.generationDuration;
		preferredStems = other.preferredStems;
		stems = other.stems;
		loopStart = other.loopStart;
		loopEnd = other.loopEnd;
		useOriginalFile = other.useOriginalFile.load();
		hasOriginalVersion = other.hasOriginalVersion.load();
		originalStagingBuffer = other.originalStagingBuffer;
		isLoaded = other.isLoaded.load();
		isLoading = other.isLoading.load();
	}

	void reset() {
		audioBuffer.setSize(0, 0);
		audioFilePath.clear();
		numSamples = 0;
		sampleRate = 48000.0;
		originalBpm = 126.0f;
		prompt.clear();
		selectedPrompt.clear();
		generationPrompt.clear();
		generationBpm = 126.0f;
		generationKey.clear();
		generationDuration = 6;
		preferredStems.clear();
		stems.clear();
		loopStart = 0.0;
		loopEnd = 4.0;
		useOriginalFile = false;
		hasOriginalVersion = false;
		originalStagingBuffer.setSize(0, 0);
		isLoaded = false;
		isLoading = false;
	}
};

struct TrackData
{
	juce::String trackId;
	juce::String trackName;
	int slotIndex = -1;

	TrackPage pages[4];
	int currentPageIndex = 0;
	std::atomic<bool> usePages{ false };
	int displayPageIndex = 0;
	std::atomic<bool> isPlaying{ false };
	std::atomic<bool> isArmed{ false };
	std::atomic<bool> isArmedToStop{ false };
	std::atomic<bool> isCurrentlyPlaying{ false };

	float fineOffset = 0.0f;
	std::atomic<double> cachedPlaybackRatio{ 1.0 };

	juce::AudioSampleBuffer stagingBuffer;
	std::atomic<bool> hasStagingData{ false };
	std::atomic<bool> swapRequested{ false };
	std::atomic<int> stagingNumSamples{ 0 };
	std::atomic<double> stagingSampleRate{ 48000.0 };
	float stagingOriginalBpm = 126.0f;

	int timeStretchMode = 4;
	double timeStretchRatio = 1.0;
	double bpmOffset = 0.0;
	int midiNote = 60;

	std::atomic<bool> isEnabled{ true };
	std::atomic<bool> isSolo{ false };
	std::atomic<bool> isMuted{ false };
	std::atomic<bool> loopPointsLocked{ false };
	std::atomic<float> volume{ 0.8f };
	std::atomic<float> pan{ 0.0f };

	float bpm = 126.0f;
	std::atomic<double> readPosition{ 0.0 };

	bool showWaveform = false;
	bool showSequencer = false;

	juce::AudioSampleBuffer audioBuffer;
	juce::String audioFilePath;
	double sampleRate = 48000.0;
	int numSamples = 0;
	double loopStart = 0.0;
	double loopEnd = 4.0;
	float originalBpm = 126.0f;
	juce::String prompt;
	juce::String style;
	juce::String stems;
	juce::String generationPrompt;
	float generationBpm;
	juce::String generationKey;
	int generationDuration;
	std::vector<juce::String> preferredStems = {};
	juce::String selectedPrompt;
	std::atomic<bool> useOriginalFile{ false };
	std::atomic<bool> hasOriginalVersion{ false };
	std::atomic<bool> nextHasOriginalVersion{ false };
	juce::AudioBuffer<float> originalStagingBuffer;

	bool isVersionSwitch = false;
	double preservedLoopStart = 0.0;
	double preservedLoopEnd = 4.0;
	bool preservedLoopLocked = false;

	std::atomic<bool> randomRetriggerEnabled{ false };
	std::atomic<int> randomRetriggerInterval{ 3 };
	std::atomic<double> lastRetriggerTime{ -1.0 };
	std::atomic<double> nextRetriggerTime{ 0.0 };
	std::atomic<bool> randomRetriggerActive{ false };
	std::atomic<bool> beatRepeatActive{ false };
	std::atomic<double> beatRepeatStartPosition{ 0.0 };
	std::atomic<double> beatRepeatEndPosition{ 0.0 };
	std::atomic<double> beatRepeatDuration{ 0.25 };
	std::atomic<double> originalReadPosition{ 0.0 };
	std::atomic<bool> beatRepeatPending{ false };
	std::atomic<double> lastBeatTime{ -1.0 };
	std::atomic<bool> beatRepeatStopPending{ false };
	std::atomic<bool> randomRetriggerDurationEnabled{ false };
	std::atomic<int64_t> pendingBeatNumber{ -1 };
	std::atomic<int64_t> pendingStopBeatNumber{ -1 };

	int customStepCounter = 0;
	double lastPpqPosition = -1.0;

	juce::String currentSampleId;

	std::function<void(bool)> onPlayStateChanged;
	std::function<void(bool)> onArmedStateChanged;
	std::function<void(bool)> onArmedToStopStateChanged;

	enum class PendingAction
	{
		None,
		StartOnNextMeasure,
		StopOnNextMeasure
	};

	PendingAction pendingAction = PendingAction::None;

	struct SequencerData
	{
		bool steps[4][16] = {};
		float velocities[4][16] = {};
		bool isPlaying = false;
		int currentStep = 0;
		int currentMeasure = 0;
		int numMeasures = 1;
		int beatsPerMeasure = 4;
		double stepAccumulator = 0.0;
		double samplesPerStep = 0.0;
		int stepPages[4][16] = {};
	} sequencerData{};

	TrackData() : trackId(juce::Uuid().toString())
	{
		volume = 0.8f;
		pan = 0.0f;
		readPosition = 0.0;
		bpmOffset = 0.0;
		onPlayStateChanged = nullptr;

		for (int i = 0; i < 4; ++i) {
			pages[i].reset();
		}
	}

	~TrackData()
	{
		onPlayStateChanged = nullptr;
		onArmedStateChanged = nullptr;
		onArmedToStopStateChanged = nullptr;
	}

	TrackPage& getCurrentPage() {
		return pages[currentPageIndex];
	}

	const TrackPage& getCurrentPage() const {
		return pages[currentPageIndex];
	}

	void syncLegacyProperties() {
		if (!usePages) return;

		auto& currentPage = getCurrentPage();

		audioBuffer = currentPage.audioBuffer;
		audioFilePath = currentPage.audioFilePath;
		numSamples = currentPage.numSamples;
		sampleRate = currentPage.sampleRate;
		originalBpm = currentPage.originalBpm;

		loopStart = currentPage.loopStart;
		loopEnd = currentPage.loopEnd;

		prompt = currentPage.prompt;
		selectedPrompt = currentPage.selectedPrompt;
		generationPrompt = currentPage.generationPrompt;
		generationBpm = currentPage.generationBpm;
		generationKey = currentPage.generationKey;
		generationDuration = currentPage.generationDuration;
		preferredStems = currentPage.preferredStems;
		stems = currentPage.stems;

		useOriginalFile = currentPage.useOriginalFile.load();
		hasOriginalVersion = currentPage.hasOriginalVersion.load();
		originalStagingBuffer = currentPage.originalStagingBuffer;

		DBG("Synced legacy properties - loops: " << loopStart << " to " << loopEnd);
	}

	void migrateToPages() {
		if (usePages) return;

		pages[0].audioBuffer = audioBuffer;
		pages[0].audioFilePath = audioFilePath;
		pages[0].numSamples = numSamples;
		pages[0].sampleRate = sampleRate;
		pages[0].originalBpm = originalBpm;
		pages[0].loopStart = loopStart;
		pages[0].loopEnd = loopEnd;
		pages[0].prompt = prompt;
		pages[0].selectedPrompt = selectedPrompt;
		pages[0].generationPrompt = generationPrompt;
		pages[0].generationBpm = generationBpm;
		pages[0].generationKey = generationKey;
		pages[0].generationDuration = generationDuration;
		pages[0].preferredStems = preferredStems;
		pages[0].stems = stems;
		pages[0].useOriginalFile = useOriginalFile.load();
		pages[0].hasOriginalVersion = hasOriginalVersion.load();
		pages[0].originalStagingBuffer = originalStagingBuffer;
		pages[0].isLoaded = (numSamples > 0);

		currentPageIndex = 0;
		displayPageIndex = 0;
		usePages = true;

		DBG("Track " << trackName << " migrated to pages system");
	}

	DjIaClient::LoopRequest createLoopRequestForDisplayPage(int currentDisplayPageIndex) const
	{
		DjIaClient::LoopRequest request;
		if (usePages && currentDisplayPageIndex >= 0 && currentDisplayPageIndex < 4) {
			const auto& targetPage = pages[currentDisplayPageIndex];
			request.prompt = !targetPage.selectedPrompt.isEmpty() ? targetPage.selectedPrompt : targetPage.generationPrompt;
			request.bpm = targetPage.generationBpm;
			request.key = targetPage.generationKey;
			request.generationDuration = static_cast<float>(targetPage.generationDuration);
			request.preferredStems = targetPage.preferredStems;
		}
		else {
			request.prompt = !selectedPrompt.isEmpty() ? selectedPrompt : generationPrompt;
			request.bpm = generationBpm;
			request.key = generationKey;
			request.generationDuration = static_cast<float>(generationDuration);
			request.preferredStems = preferredStems;
		}
		return request;
	}

	void setCurrentPage(int pageIndex) {
		if (pageIndex < 0 || pageIndex >= 4) return;
		if (currentPageIndex == pageIndex) return;

		currentPageIndex = pageIndex;

		if (usePages) {
			syncLegacyProperties();
		}

		DBG("Track " << trackName << " switched to page " << (char)('A' + pageIndex) <<
			" - loops: " << getCurrentPage().loopStart << " to " << getCurrentPage().loopEnd);
	}

	DjIaClient::LoopRequest createLoopRequest() const
	{
		DjIaClient::LoopRequest request;
		if (usePages) {
			const auto& currentPage = getCurrentPage();
			request.prompt = !currentPage.selectedPrompt.isEmpty() ? currentPage.selectedPrompt : currentPage.generationPrompt;
			request.bpm = currentPage.generationBpm;
			request.key = currentPage.generationKey;
			request.generationDuration = static_cast<float>(currentPage.generationDuration);
			request.preferredStems = currentPage.preferredStems;
		}
		else {
			request.prompt = !selectedPrompt.isEmpty() ? selectedPrompt : generationPrompt;
			request.bpm = generationBpm;
			request.key = generationKey;
			request.generationDuration = static_cast<float>(generationDuration);
			request.preferredStems = preferredStems;
		}
		return request;
	}

	void updateFromRequest(const DjIaClient::LoopRequest& request)
	{
		if (usePages) {
			auto& currentPage = getCurrentPage();
			currentPage.generationPrompt = request.prompt;
			currentPage.generationBpm = request.bpm;
			currentPage.generationKey = request.key;
			currentPage.generationDuration = static_cast<int>(request.generationDuration);
			currentPage.preferredStems = request.preferredStems;
			syncLegacyProperties();
		}
		else {
			generationPrompt = request.prompt;
			generationBpm = request.bpm;
			generationKey = request.key;
			generationDuration = static_cast<int>(request.generationDuration);
			preferredStems = request.preferredStems;
		}
	}

	void reset()
	{
		if (usePages) {
			for (int i = 0; i < 4; ++i) {
				pages[i].reset();
			}
			currentPageIndex = 0;
			syncLegacyProperties();
		}
		else {
			audioBuffer.setSize(0, 0);
			numSamples = 0;
			readPosition = 0.0;
			isEnabled = true;
			isMuted = false;
			isSolo = false;
			loopPointsLocked = false;
			volume = 0.8f;
			pan = 0.0f;
			bpmOffset = 0.0;
			useOriginalFile = false;
			hasOriginalVersion = false;
			originalStagingBuffer.setSize(0, 0);
			isVersionSwitch = false;
			preservedLoopStart = 0.0;
			preservedLoopEnd = 4.0;
			preservedLoopLocked = false;
		}
	}

	void setPlaying(bool playing)
	{
		bool wasPlaying = isPlaying.load();
		isPlaying = playing;
		if (wasPlaying != playing && onPlayStateChanged && getCurrentAudioBuffer().getNumChannels() > 0 && isPlaying.load())
		{
			juce::MessageManager::callAsync([this, playing]()
				{
					if (onPlayStateChanged) {
						onPlayStateChanged(playing);
					}
				});
		}
	}

	void setArmed(bool armed)
	{
		bool wasArmed = isArmed.load();
		isArmed = armed;
		if (wasArmed != armed && onArmedStateChanged && getCurrentAudioBuffer().getNumChannels() > 0 && isPlaying.load())
		{
			juce::MessageManager::callAsync([this, armed]()
				{
					if (onArmedStateChanged) {
						onArmedStateChanged(armed);
					}
				});
		}
	}

	void setArmedToStop(bool armedToStop)
	{
		isArmedToStop = armedToStop;
		if (onArmedToStopStateChanged && getCurrentAudioBuffer().getNumChannels() > 0 && isCurrentlyPlaying.load())
		{
			juce::MessageManager::callAsync([this, armedToStop]()
				{
					if (onArmedToStopStateChanged) {
						onArmedToStopStateChanged(armedToStop);
					}
				});
		}
	}

	void setStop()
	{
		juce::MessageManager::callAsync([this]()
			{
				if (onPlayStateChanged) {
					onPlayStateChanged(false);
				}
			});
	}

private:
	juce::AudioSampleBuffer& getCurrentAudioBuffer() {
		return usePages ? pages[currentPageIndex].audioBuffer : audioBuffer;
	}
};