/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (C) 2025 Anthony Charretier
 */

 // "Please DON'T download this if you're a real musician"
 // - Dedicated to those who downloaded it anyway

#include "./PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include "SequencerComponent.h"
#include "version.h"
#include "ColourPalette.h"

DjIaVstEditor::DjIaVstEditor(DjIaVstProcessor& p)
	: AudioProcessorEditor(&p), audioProcessor(p)
{
	setSize(1300, 800);
	setWantsKeyboardFocus(true);
	tooltipWindow = std::make_unique<juce::TooltipWindow>(this, 700);
	logoImage = juce::ImageCache::getFromMemory(BinaryData::logo_png,
		BinaryData::logo_pngSize);
	bannerImage = juce::ImageCache::getFromMemory(BinaryData::cyber_banner_png,
		BinaryData::cyber_banner_pngSize);
	audioProcessor.setGenerationListener(this);
	if (audioProcessor.isStateReady())
	{
		initUI();
	}
	else
		startTimer(50);

	juce::Timer::callAfterDelay(300, [this]()
		{
			loadPromptPresets();
			refreshTracks();
			for (auto& trackComp : trackComponents)
			{
				if (trackComp->getTrack() && trackComp->getTrack()->showWaveform)
				{
					trackComp->toggleWaveformDisplay();
				}
				if (trackComp->getTrack() && trackComp->getTrack()->showSequencer)
				{
					trackComp->toggleSequencerDisplay();
				}
			}
			if (audioProcessor.getIsGenerating())
			{
				generateButton.setEnabled(false);
				setAllGenerateButtonsEnabled(false);
				statusLabel.setText("Generation in progress...", juce::dontSendNotification);
				juce::String generatingId = audioProcessor.getGeneratingTrackId();
				for (auto& trackComp : trackComponents)
				{
					if (trackComp->getTrackId() == generatingId)
					{
						trackComp->startGeneratingAnimation();
						break;
					}
				}
			} });
}

DjIaVstEditor::~DjIaVstEditor()
{
	audioProcessor.onUIUpdateNeeded = nullptr;
	audioProcessor.setGenerationListener(nullptr);
	audioProcessor.getMidiLearnManager().registerUICallback("promptPresetSelector", nullptr);
}

void DjIaVstEditor::updateMidiIndicator(const juce::String& noteInfo)
{
	lastMidiNote = noteInfo;

	juce::MessageManager::callAsync([this, noteInfo]()
		{
			if (midiIndicator.isShowing())
			{
				midiIndicator.setText(noteInfo, juce::dontSendNotification);
				auto greenWithOpacity = ColourPalette::textSuccess.withAlpha(0.3f);
				midiIndicator.setColour(juce::Label::backgroundColourId, greenWithOpacity);

				juce::Timer::callAfterDelay(200, [this]()
					{
						if (midiIndicator.isShowing())
						{
							midiIndicator.setColour(juce::Label::backgroundColourId, ColourPalette::backgroundDeep);
						}
					});
			} });
}

void DjIaVstEditor::updateUIComponents()
{
	if (!isGenerating.load() && audioProcessor.getIsGenerating())
	{
		isGenerating.store(true);
		wasGenerating.store(true);
		startGenerationButtonAnimation();
		startTimer(200);
	}
	for (auto& trackComp : trackComponents)
	{
		if (trackComp->isShowing())
		{
			TrackData* track = audioProcessor.getTrack(trackComp->getTrackId());
			if (track && !trackComp->isEditingLabel)
			{
				trackComp->updateFromTrackData();
			}
		}
	}
	if (mixerPanel)
	{
		mixerPanel->updateAllMixerComponents();
	}

	if (!lastMidiNote.isEmpty())
	{
		static int midiBlinkCounter = 0;
		if (++midiBlinkCounter > 6)
		{
			midiIndicator.setColour(juce::Label::backgroundColourId, ColourPalette::backgroundDeep);
			lastMidiNote.clear();
			midiBlinkCounter = 0;
		}
	}

	if (!autoLoadButton.getToggleState())
	{
		updateLoadButtonState();
	}

	for (auto& trackComp : trackComponents)
	{
		TrackData* track = audioProcessor.getTrack(trackComp->getTrackId());
		if (track && track->isPlaying.load() && track->numSamples > 0)
		{
			double startSample = track->loopStart * track->sampleRate;
			double currentTimeInSection = (startSample + track->readPosition.load()) / track->sampleRate;

			trackComp->updatePlaybackPosition(currentTimeInSection);
		}
	}

	static bool currentWasGenerating = false;
	bool isCurrentlyGenerating = generateButton.isEnabled() == false;
	if (currentWasGenerating && !isCurrentlyGenerating)
	{
		for (auto& trackComp : trackComponents)
		{
			trackComp->refreshWaveformIfNeeded();
		}
	}
	currentWasGenerating = isCurrentlyGenerating;
}

void DjIaVstEditor::onGenerationComplete(const juce::String& trackId, const juce::String& message)
{
	bool isError = message.startsWith("ERROR:");
	stopGenerationUI(trackId, !isError, isError ? message : "");
	statusLabel.setText(message, juce::dontSendNotification);

	if (isError)
	{
		statusLabel.setColour(juce::Label::textColourId, ColourPalette::textDanger);
		juce::Timer::callAfterDelay(5000, [this]()
			{
				statusLabel.setText("Ready", juce::dontSendNotification);
				statusLabel.setColour(juce::Label::textColourId, ColourPalette::textSuccess); });
	}
	else
	{
		statusLabel.setColour(juce::Label::textColourId, ColourPalette::textSuccess);
		juce::Timer::callAfterDelay(3000, [this]()
			{
				statusLabel.setText("Ready", juce::dontSendNotification);
				statusLabel.setColour(juce::Label::textColourId, ColourPalette::textSuccess); });
	}
}

void DjIaVstEditor::refreshTracks()
{
	trackComponents.clear();
	tracksContainer.removeAllChildren();

	refreshTrackComponents();
	updateSelectedTrack();
	repaint();
}

void DjIaVstEditor::initUI()
{
	setupUI();
	refreshUIForMode();
	serverUrlInput.setText(audioProcessor.getServerUrl(), juce::dontSendNotification);
	apiKeyInput.setText(audioProcessor.getApiKey(), juce::dontSendNotification);
	if (audioProcessor.getServerUrl().isEmpty())
	{
		juce::Timer::callAfterDelay(500, [this]()
			{ showFirstTimeSetup(); });
	}
	isInitialized.store(true);
	juce::WeakReference<DjIaVstEditor> weakThis(this);
	audioProcessor.setMidiIndicatorCallback([weakThis](const juce::String& noteInfo)
		{
			if (weakThis != nullptr) {
				weakThis->updateMidiIndicator(noteInfo);
			} });
			loadPromptPresets();
			refreshTracks();
			audioProcessor.onUIUpdateNeeded = [this]()
				{
					juce::MessageManager::callAsync([this]()
						{ updateUIComponents(); });
				};
}

void DjIaVstEditor::showFirstTimeSetup()
{
	auto alertWindow = std::make_unique<juce::AlertWindow>(
		"OBSIDIAN-Neural Configuration",
		"Choose your generation method:",
		juce::MessageBoxIconType::InfoIcon);

	juce::StringArray modes;
	modes.add("Server/API (Full features + stems separation)");
	modes.add("Local Model (Basic - requires manual setup)");
	alertWindow->addComboBox("generationMode", modes, "Generation Mode:");
	if (auto* modeCombo = alertWindow->getComboBoxComponent("generationMode"))
	{
		modeCombo->setSelectedItemIndex(audioProcessor.getUseLocalModel() ? 1 : 0);
	}

	alertWindow->addTextEditor("serverUrl",
		audioProcessor.getServerUrl().isEmpty() ? "http://localhost:8000" : audioProcessor.getServerUrl(),
		"Server URL:");
	alertWindow->addTextEditor("apiKey", "", "API Key:");
	if (auto* apiKeyEditor = alertWindow->getTextEditor("apiKey"))
	{
		apiKeyEditor->setPasswordCharacter('*');
	}

	juce::StringArray timeouts = { "1 minute", "2 minutes", "5 minutes", "10 minutes", "15 minutes", "20 minutes", "30 minutes", "45 minutes" };
	alertWindow->addComboBox("requestTimeout", timeouts, "Request Timeout:");
	if (auto* timeoutCombo = alertWindow->getComboBoxComponent("requestTimeout"))
	{
		timeoutCombo->setSelectedItemIndex(2);
	}

	alertWindow->addButton("Save & Continue", 1);
	alertWindow->addButton("Skip for now", 0);

	auto* windowPtr = alertWindow.get();
	alertWindow.release()->enterModalState(true, juce::ModalCallbackFunction::create([this, windowPtr](int result)
		{
			if (result == 1) {
				auto* modeCombo = windowPtr->getComboBoxComponent("generationMode");
				auto* urlEditor = windowPtr->getTextEditor("serverUrl");
				auto* keyEditor = windowPtr->getTextEditor("apiKey");
				auto* timeoutCombo = windowPtr->getComboBoxComponent("requestTimeout");

				if (modeCombo && urlEditor && keyEditor && timeoutCombo) {
					bool useLocal = (modeCombo->getSelectedItemIndex() == 1);
					audioProcessor.setUseLocalModel(useLocal);

					if (useLocal) {
						checkLocalModelsAndNotify();
					}
					else {
						audioProcessor.setServerUrl(urlEditor->getText());
						audioProcessor.setApiKey(keyEditor->getText());
					}

					juce::Array<int> timeoutMinutes = { 1, 2, 5, 10, 15, 20, 30, 45 };
					int selectedTimeoutMs = timeoutMinutes[timeoutCombo->getSelectedItemIndex()] * 60 * 1000;
					audioProcessor.setRequestTimeout(selectedTimeoutMs);
					audioProcessor.saveGlobalConfig();

					if (!useLocal) {
						statusLabel.setText("Configuration saved!", juce::dontSendNotification);
					}
					refreshUIForMode();
				}
			}
			windowPtr->exitModalState(result);
			delete windowPtr; }));
}

void DjIaVstEditor::refreshUIForMode()
{
	bool isLocalMode = audioProcessor.getUseLocalModel();

	stemsLabel.setEnabled(!isLocalMode);
	drumsButton.setEnabled(!isLocalMode);
	bassButton.setEnabled(!isLocalMode);
	otherButton.setEnabled(!isLocalMode);
	vocalsButton.setEnabled(!isLocalMode);
	guitarButton.setEnabled(!isLocalMode);
	pianoButton.setEnabled(!isLocalMode);
	durationSlider.setEnabled(!isLocalMode);
	durationLabel.setEnabled(!isLocalMode);

	resized();
}

void DjIaVstEditor::showConfigDialog()
{
	auto alertWindow = std::make_unique<juce::AlertWindow>(
		"Update Configuration",
		"Update your settings:",
		juce::MessageBoxIconType::QuestionIcon);

	juce::StringArray modes;
	modes.add("Server/API (Full features + stems separation)");
	modes.add("Local Model (Basic - requires manual setup)");
	alertWindow->addComboBox("generationMode", modes, "Generation Mode:");
	if (auto* modeCombo = alertWindow->getComboBoxComponent("generationMode"))
	{
		modeCombo->setSelectedItemIndex(audioProcessor.getUseLocalModel() ? 1 : 0);
	}

	alertWindow->addTextEditor("serverUrl", audioProcessor.getServerUrl(), "Server URL:");
	alertWindow->addTextEditor("apiKey", "", "API Key (leave blank to keep current):");
	if (auto* apiKeyEditor = alertWindow->getTextEditor("apiKey"))
	{
		apiKeyEditor->setPasswordCharacter('*');
	}

	juce::StringArray timeouts = { "1 minute", "2 minutes", "5 minutes", "10 minutes", "15 minutes", "20 minutes", "30 minutes", "45 minutes" };
	alertWindow->addComboBox("requestTimeout", timeouts, "Request Timeout:");
	if (auto* timeoutCombo = alertWindow->getComboBoxComponent("requestTimeout"))
	{
		int currentTimeoutMs = audioProcessor.getRequestTimeout();
		int currentTimeoutMinutes = currentTimeoutMs / (60 * 1000);
		juce::Array<int> timeoutValues = { 1, 2, 5, 10, 15, 20, 30, 45 };
		int selectedIndex = 2;
		for (int i = 0; i < timeoutValues.size(); ++i)
		{
			if (timeoutValues[i] == currentTimeoutMinutes)
			{
				selectedIndex = i;
				break;
			}
		}
		timeoutCombo->setSelectedItemIndex(selectedIndex);
	}

	alertWindow->addButton("Update", 1);
	alertWindow->addButton("Cancel", 0);

	auto* windowPtr = alertWindow.get();
	alertWindow.release()->enterModalState(true, juce::ModalCallbackFunction::create([this, windowPtr](int result)
		{
			if (result == 1) {
				auto* modeCombo = windowPtr->getComboBoxComponent("generationMode");
				auto* urlEditor = windowPtr->getTextEditor("serverUrl");
				auto* keyEditor = windowPtr->getTextEditor("apiKey");
				auto* timeoutCombo = windowPtr->getComboBoxComponent("requestTimeout");

				if (modeCombo && urlEditor && keyEditor && timeoutCombo) {
					bool useLocal = (modeCombo->getSelectedItemIndex() == 1);
					bool modeChanged = (useLocal != audioProcessor.getUseLocalModel());

					audioProcessor.setUseLocalModel(useLocal);

					if (useLocal) {
						checkLocalModelsAndNotify();
					}
					else {
						audioProcessor.setServerUrl(urlEditor->getText());
						juce::String newKey = keyEditor->getText();
						if (newKey.isNotEmpty()) {
							audioProcessor.setApiKey(newKey);
						}
					}

					juce::Array<int> timeoutMinutes = { 1, 2, 5, 10, 15, 20, 30, 45 };
					int selectedTimeoutMs = timeoutMinutes[timeoutCombo->getSelectedItemIndex()] * 60 * 1000;
					audioProcessor.setRequestTimeout(selectedTimeoutMs);

					audioProcessor.saveGlobalConfig();

					if (modeChanged) {
						refreshUIForMode();
						statusLabel.setText("Mode changed! Configuration updated.", juce::dontSendNotification);
					}
					else {
						statusLabel.setText("Configuration updated!", juce::dontSendNotification);
					}

					statusLabel.setColour(juce::Label::textColourId, ColourPalette::textSuccess);

					juce::Timer::callAfterDelay(3000, [this]() {
						statusLabel.setText("Ready", juce::dontSendNotification);
						statusLabel.setColour(juce::Label::textColourId, ColourPalette::textSuccess);
						});
				}
			}

			windowPtr->exitModalState(result);
			delete windowPtr; }));
}

void DjIaVstEditor::checkLocalModelsAndNotify()
{
	auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
		.getChildFile("OBSIDIAN-Neural");
	auto stableAudioDir = appDataDir.getChildFile("stable-audio");

	StableAudioEngine tempEngine;
	bool modelsPresent = tempEngine.initialize(stableAudioDir.getFullPathName());

	if (modelsPresent)
	{
		statusLabel.setText("Local models found! Configuration saved.", juce::dontSendNotification);
		statusLabel.setColour(juce::Label::textColourId, ColourPalette::textSuccess);
	}
	else
	{
		juce::AlertWindow::showAsync(
			juce::MessageBoxOptions()
			.withIconType(juce::MessageBoxIconType::InfoIcon)
			.withTitle("Local Models Required")
			.withMessage("Local models not found!\n\n"
				"You need to download and setup the required model files.\n"
				"Please follow the setup instructions on GitHub.\n\n"
				"Expected location: " +
				stableAudioDir.getFullPathName())
			.withButton("Open GitHub Instructions")
			.withButton("OK"),
			[this](int result)
			{
				if (result == 1)
				{
					juce::URL githubUrl("https://github.com/innermost47/ai-dj/blob/main/README.md");
					githubUrl.launchInDefaultBrowser();
				}
			});

		statusLabel.setText("Local mode selected - Models setup required", juce::dontSendNotification);
		statusLabel.setColour(juce::Label::textColourId, ColourPalette::textDanger);
	}
}

void DjIaVstEditor::timerCallback()
{
	if (!isInitialized.load())
	{
		if (audioProcessor.isStateReady())
		{
			stopTimer();
			initUI();
		}
		bool anyTrackPlaying = false;

		for (auto& trackComp : trackComponents)
		{
			if (trackComp->isShowing())
			{
				TrackData* track = audioProcessor.getTrack(trackComp->getTrackId());
				if (track && track->isPlaying.load())
				{
					trackComp->updateFromTrackData();
					anyTrackPlaying = true;
				}
			}
		}

		if (!anyTrackPlaying)
		{
			static int skipFrames = 0;
			skipFrames++;
			if (skipFrames < 10)
				return;
			skipFrames = 0;
		}

		static double lastHostBpm = 0.0;
		double currentHostBpm = audioProcessor.getHostBpm();

		if (std::abs(currentHostBpm - lastHostBpm) > 0.1)
		{
			lastHostBpm = currentHostBpm;
			for (auto& trackComp : trackComponents)
			{
				TrackData* track = audioProcessor.getTrack(trackComp->getTrackId());
				if (track && (track->timeStretchMode == 3 || track->timeStretchMode == 4))
				{
					trackComp->updateWaveformWithTimeStretch();
				}
			}
		}
	}
	else
	{
		if (isButtonBlinking)
		{
			blinkCounter++;
			if (blinkCounter % 3 == 0)
			{
				auto currentColor = generateButton.findColour(juce::TextButton::buttonColourId);
				bool isWarning = (currentColor == ColourPalette::buttonWarning);

				generateButton.setColour(juce::TextButton::buttonColourId,
					isWarning ? ColourPalette::buttonSuccess : ColourPalette::buttonWarning);
			}
		}
	}
}

void DjIaVstEditor::startGenerationButtonAnimation()
{
	if (!isButtonBlinking)
	{
		originalButtonText = generateButton.getButtonText();
		generateButton.setEnabled(false);
		generateButton.setButtonText("Generating Track...");
		generateButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonWarning);
		isButtonBlinking = true;
		blinkCounter = 0;
	}
}

void DjIaVstEditor::stopGenerationButtonAnimation()
{
	if (isButtonBlinking)
	{
		generateButton.setEnabled(true);
		generateButton.setButtonText(originalButtonText);
		generateButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonSuccess);
		isButtonBlinking = false;
		generatingTrackId.clear();
	}
}

void DjIaVstEditor::setupUI()
{
	getLookAndFeel().setColour(juce::TextButton::buttonColourId, ColourPalette::backgroundLight);
	getLookAndFeel().setColour(juce::TextButton::textColourOffId, ColourPalette::textPrimary);
	getLookAndFeel().setColour(juce::ComboBox::backgroundColourId, ColourPalette::backgroundDark);
	getLookAndFeel().setColour(juce::ComboBox::textColourId, ColourPalette::textPrimary);
	getLookAndFeel().setColour(juce::TextEditor::backgroundColourId, ColourPalette::backgroundDeep);
	getLookAndFeel().setColour(juce::TextEditor::textColourId, ColourPalette::textPrimary);
	getLookAndFeel().setColour(juce::Slider::backgroundColourId, ColourPalette::backgroundDark);
	getLookAndFeel().setColour(juce::Slider::thumbColourId, ColourPalette::sliderThumb);
	getLookAndFeel().setColour(juce::Slider::trackColourId, ColourPalette::sliderTrack);

	addAndMakeVisible(nextTrackButton);
	nextTrackButton.setButtonText("Next Track");
	nextTrackButton.setTooltip("Select next track (Right-click for MIDI learn)");
	nextTrackButton.setDescription("Next Track");

	addAndMakeVisible(prevTrackButton);
	prevTrackButton.setButtonText("Prev Track");
	prevTrackButton.setTooltip("Select previous track (Right-click for MIDI learn)");
	prevTrackButton.setDescription("Previous Track");

	addAndMakeVisible(pluginNameLabel);
	pluginNameLabel.setText("NEURAL SOUND ENGINE", juce::dontSendNotification);
	pluginNameLabel.setFont(juce::FontOptions("Courier New", 22.0f, juce::Font::bold));
	pluginNameLabel.setColour(juce::Label::textColourId, ColourPalette::textAccent);
	pluginNameLabel.setJustificationType(juce::Justification::left);

	addAndMakeVisible(developerLabel);
	developerLabel.setText("Developed by InnerMost47", juce::dontSendNotification);
	developerLabel.setFont(juce::FontOptions("Courier New", 14.0f, juce::Font::italic));
	developerLabel.setColour(juce::Label::textColourId, ColourPalette::textPrimary);
	developerLabel.setJustificationType(juce::Justification::left);

	addAndMakeVisible(stabilityLabel);
	stabilityLabel.setText("Powered by Stability AI", juce::dontSendNotification);
	stabilityLabel.setFont(juce::FontOptions("Consolas", 11.0f, juce::Font::plain));
	stabilityLabel.setColour(juce::Label::textColourId, ColourPalette::credits);
	stabilityLabel.setJustificationType(juce::Justification::left);

	menuBar = std::make_unique<juce::MenuBarComponent>(this);
	addAndMakeVisible(*menuBar);
	addAndMakeVisible(promptPresetSelector);

	addAndMakeVisible(savePresetButton);
	savePresetButton.setButtonText(juce::String::fromUTF8("\xE2\x9C\x93"));

	addAndMakeVisible(promptInput);
	promptInput.setMultiLine(false);
	promptInput.setTextToShowWhenEmpty("Enter custom prompt or select preset...", ColourPalette::textSecondary);
	promptInput.setText(audioProcessor.getGlobalPrompt(), juce::dontSendNotification);

	addAndMakeVisible(resetUIButton);
	resetUIButton.setButtonText("Reset UI");
	resetUIButton.setColour(juce::TextButton::buttonColourId, ColourPalette::amber);
	resetUIButton.setColour(juce::TextButton::textColourOffId, ColourPalette::textPrimary);
	resetUIButton.setTooltip("Reset UI state if stuck in generation mode");

	addAndMakeVisible(keySelector);
	keySelector.addItem("C Ionian", 1);
	keySelector.addItem("C# Ionian", 2);
	keySelector.addItem("D Ionian", 3);
	keySelector.addItem("D# Ionian", 4);
	keySelector.addItem("E Ionian", 5);
	keySelector.addItem("F Ionian", 6);
	keySelector.addItem("F# Ionian", 7);
	keySelector.addItem("G Ionian", 8);
	keySelector.addItem("G# Ionian", 9);
	keySelector.addItem("A Ionian", 10);
	keySelector.addItem("A# Ionian", 11);
	keySelector.addItem("B Ionian", 12);
	keySelector.addItem("C Dorian", 13);
	keySelector.addItem("C# Dorian", 14);
	keySelector.addItem("D Dorian", 15);
	keySelector.addItem("D# Dorian", 16);
	keySelector.addItem("E Dorian", 17);
	keySelector.addItem("F Dorian", 18);
	keySelector.addItem("F# Dorian", 19);
	keySelector.addItem("G Dorian", 20);
	keySelector.addItem("G# Dorian", 21);
	keySelector.addItem("A Dorian", 22);
	keySelector.addItem("A# Dorian", 23);
	keySelector.addItem("B Dorian", 24);
	keySelector.addItem("C Phrygian", 25);
	keySelector.addItem("C# Phrygian", 26);
	keySelector.addItem("D Phrygian", 27);
	keySelector.addItem("D# Phrygian", 28);
	keySelector.addItem("E Phrygian", 29);
	keySelector.addItem("F Phrygian", 30);
	keySelector.addItem("F# Phrygian", 31);
	keySelector.addItem("G Phrygian", 32);
	keySelector.addItem("G# Phrygian", 33);
	keySelector.addItem("A Phrygian", 34);
	keySelector.addItem("A# Phrygian", 35);
	keySelector.addItem("B Phrygian", 36);
	keySelector.addItem("C Lydian", 37);
	keySelector.addItem("C# Lydian", 38);
	keySelector.addItem("D Lydian", 39);
	keySelector.addItem("D# Lydian", 40);
	keySelector.addItem("E Lydian", 41);
	keySelector.addItem("F Lydian", 42);
	keySelector.addItem("F# Lydian", 43);
	keySelector.addItem("G Lydian", 44);
	keySelector.addItem("G# Lydian", 45);
	keySelector.addItem("A Lydian", 46);
	keySelector.addItem("A# Lydian", 47);
	keySelector.addItem("B Lydian", 48);
	keySelector.addItem("C Mixolydian", 49);
	keySelector.addItem("C# Mixolydian", 50);
	keySelector.addItem("D Mixolydian", 51);
	keySelector.addItem("D# Mixolydian", 52);
	keySelector.addItem("E Mixolydian", 53);
	keySelector.addItem("F Mixolydian", 54);
	keySelector.addItem("F# Mixolydian", 55);
	keySelector.addItem("G Mixolydian", 56);
	keySelector.addItem("G# Mixolydian", 57);
	keySelector.addItem("A Mixolydian", 58);
	keySelector.addItem("A# Mixolydian", 59);
	keySelector.addItem("B Mixolydian", 60);
	keySelector.addItem("C Aeolian", 61);
	keySelector.addItem("C# Aeolian", 62);
	keySelector.addItem("D Aeolian", 63);
	keySelector.addItem("D# Aeolian", 64);
	keySelector.addItem("E Aeolian", 65);
	keySelector.addItem("F Aeolian", 66);
	keySelector.addItem("F# Aeolian", 67);
	keySelector.addItem("G Aeolian", 68);
	keySelector.addItem("G# Aeolian", 69);
	keySelector.addItem("A Aeolian", 70);
	keySelector.addItem("A# Aeolian", 71);
	keySelector.addItem("B Aeolian", 72);
	keySelector.addItem("C Locrian", 73);
	keySelector.addItem("C# Locrian", 74);
	keySelector.addItem("D Locrian", 75);
	keySelector.addItem("D# Locrian", 76);
	keySelector.addItem("E Locrian", 77);
	keySelector.addItem("F Locrian", 78);
	keySelector.addItem("F# Locrian", 79);
	keySelector.addItem("G Locrian", 80);
	keySelector.addItem("G# Locrian", 81);
	keySelector.addItem("A Locrian", 82);
	keySelector.addItem("A# Locrian", 83);
	keySelector.addItem("B Locrian", 84);
	keySelector.addItem("C Major", 85);
	keySelector.addItem("C# Major", 86);
	keySelector.addItem("D Major", 87);
	keySelector.addItem("D# Major", 88);
	keySelector.addItem("E Major", 89);
	keySelector.addItem("F Major", 90);
	keySelector.addItem("F# Major", 91);
	keySelector.addItem("G Major", 92);
	keySelector.addItem("G# Major", 93);
	keySelector.addItem("A Major", 94);
	keySelector.addItem("A# Major", 95);
	keySelector.addItem("B Major", 96);
	keySelector.addItem("C Minor", 97);
	keySelector.addItem("C# Minor", 98);
	keySelector.addItem("D Minor", 99);
	keySelector.addItem("D# Minor", 100);
	keySelector.addItem("E Minor", 101);
	keySelector.addItem("F Minor", 102);
	keySelector.addItem("F# Minor", 103);
	keySelector.addItem("G Minor", 104);
	keySelector.addItem("G# Minor", 105);
	keySelector.addItem("A Minor", 106);
	keySelector.addItem("A# Minor", 107);
	keySelector.addItem("B Minor", 108);
	keySelector.setText(audioProcessor.getGlobalKey(), juce::dontSendNotification);

	addAndMakeVisible(durationSlider);
	durationSlider.setRange(2.0, 10.0, 1.0);
	durationSlider.setValue(audioProcessor.getGlobalDuration(), juce::dontSendNotification);
	durationSlider.setColour(juce::Slider::backgroundColourId, juce::Colours::black);
	durationSlider.setColour(juce::Slider::thumbColourId, ColourPalette::sliderThumb);
	durationSlider.setColour(juce::Slider::trackColourId, ColourPalette::sliderTrack);
	durationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
	durationSlider.setTextValueSuffix(" s");
	durationSlider.setDoubleClickReturnValue(true, 6.0);

	addAndMakeVisible(durationLabel);
	durationLabel.setText("Duration", juce::dontSendNotification);

	addAndMakeVisible(generateButton);
	generateButton.setButtonText("Generate Loop");

	addAndMakeVisible(configButton);
	configButton.setButtonText(juce::String::fromUTF8("\xE2\x98\xB0"));
	configButton.setTooltip("Configure settings globally");
	configButton.onClick = [this]()
		{ showConfigDialog(); };

	addAndMakeVisible(stemsLabel);
	stemsLabel.setText("Stems:", juce::dontSendNotification);

	addAndMakeVisible(drumsButton);
	drumsButton.setButtonText("Drums");
	drumsButton.setClickingTogglesState(true);
	drumsButton.setToggleState(audioProcessor.isGlobalStemEnabled("drums"), juce::dontSendNotification);

	addAndMakeVisible(bassButton);
	bassButton.setButtonText("Bass");
	bassButton.setClickingTogglesState(true);
	bassButton.setToggleState(audioProcessor.isGlobalStemEnabled("bass"), juce::dontSendNotification);

	addAndMakeVisible(otherButton);
	otherButton.setButtonText("Other");
	otherButton.setClickingTogglesState(true);
	otherButton.setToggleState(audioProcessor.isGlobalStemEnabled("other"), juce::dontSendNotification);

	addAndMakeVisible(vocalsButton);
	vocalsButton.setButtonText("Vocals");
	vocalsButton.setClickingTogglesState(true);
	vocalsButton.setToggleState(audioProcessor.isGlobalStemEnabled("vocals"), juce::dontSendNotification);

	addAndMakeVisible(guitarButton);
	guitarButton.setButtonText("Guitar");
	guitarButton.setClickingTogglesState(true);
	guitarButton.setToggleState(audioProcessor.isGlobalStemEnabled("guitar"), juce::dontSendNotification);

	addAndMakeVisible(pianoButton);
	pianoButton.setButtonText("Piano");
	pianoButton.setClickingTogglesState(true);
	pianoButton.setToggleState(audioProcessor.isGlobalStemEnabled("piano"), juce::dontSendNotification);

	addAndMakeVisible(statusLabel);
	statusLabel.setText("Ready", juce::dontSendNotification);
	statusLabel.setColour(juce::Label::textColourId, ColourPalette::textSuccess);

	addAndMakeVisible(autoLoadButton);
	autoLoadButton.setButtonText("Auto-Load Samples");
	autoLoadButton.setClickingTogglesState(true);
	autoLoadButton.setToggleState(audioProcessor.getAutoLoadEnabled(), juce::dontSendNotification);

	addAndMakeVisible(loadSampleButton);
	loadSampleButton.setButtonText("Load Sample");
	loadSampleButton.setEnabled(!audioProcessor.getAutoLoadEnabled());

	addAndMakeVisible(midiIndicator);
	midiIndicator.setText("MIDI: Waiting...", juce::dontSendNotification);
	midiIndicator.setColour(juce::Label::backgroundColourId, ColourPalette::backgroundDeep);
	midiIndicator.setColour(juce::Label::textColourId, ColourPalette::textSuccess);
	midiIndicator.setJustificationType(juce::Justification::left);
	midiIndicator.setFont(juce::FontOptions(12.0f, juce::Font::bold));

	addAndMakeVisible(tracksLabel);
	tracksLabel.setText("Tracks:", juce::dontSendNotification);
	tracksLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));

	addAndMakeVisible(addTrackButton);
	addTrackButton.setButtonText("+ Add Track");
	addTrackButton.setColour(juce::TextButton::buttonColourId, ColourPalette::textSuccess);

	addAndMakeVisible(tracksViewport);
	tracksViewport.setViewedComponent(&tracksContainer, false);
	tracksViewport.setScrollBarsShown(true, false);
	tracksContainer.setWantsKeyboardFocus(false);
	tracksViewport.setWantsKeyboardFocus(false);
	setWantsKeyboardFocus(true);

	addAndMakeVisible(saveSessionButton);
	saveSessionButton.setButtonText("Save Session");

	addAndMakeVisible(loadSessionButton);
	loadSessionButton.setButtonText("Load Session");

	mixerPanel = std::make_unique<MixerPanel>(audioProcessor);
	addAndMakeVisible(*mixerPanel);

	sampleBankPanel = std::make_unique<SampleBankPanel>(audioProcessor);
	addChildComponent(*sampleBankPanel);
	sampleBankPanel->setVisible(false);

	addAndMakeVisible(showSampleBankButton);
	showSampleBankButton.setButtonText("Bank");
	showSampleBankButton.setColour(juce::TextButton::buttonColourId, ColourPalette::indigo);
	showSampleBankButton.setColour(juce::TextButton::textColourOffId, ColourPalette::textPrimary);
	showSampleBankButton.setTooltip("Show/hide sample bank");

	refreshTrackComponents();

	addEventListeners();

	generateButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonSuccess);
	generateButton.setColour(juce::TextButton::textColourOffId, ColourPalette::textPrimary);
	addTrackButton.setColour(juce::TextButton::buttonColourId, ColourPalette::buttonPrimary);
	loadSampleButton.setColour(juce::TextButton::buttonColourId, ColourPalette::coral);
	loadSampleButton.setColour(juce::TextButton::textColourOffId, ColourPalette::textPrimary);
	statusLabel.setColour(juce::Label::backgroundColourId, ColourPalette::backgroundDeep);
	statusLabel.setColour(juce::Label::textColourId, ColourPalette::textSuccess);

	addAndMakeVisible(bypassSequencerButton);
	bypassSequencerButton.setButtonText("Bypass Sequencer");
	bypassSequencerButton.setClickingTogglesState(true);
	bypassSequencerButton.setToggleState(audioProcessor.getBypassSequencer(), juce::dontSendNotification);
	bypassSequencerButton.setTooltip("Global bypass - direct MIDI playback for composition mode");
	bypassSequencerButton.setColour(juce::ToggleButton::textColourId, ColourPalette::textPrimary);

	promptPresetSelector.setTooltip("Select a preset prompt (Right-click for MIDI learn, Ctrl+Right-click to edit custom prompts)");
	promptInput.setTooltip("Enter your custom prompt for audio generation");
	savePresetButton.setTooltip("Save current prompt as custom preset");
	keySelector.setTooltip("Select musical key and mode for generation");
	durationSlider.setTooltip("Generation duration in seconds (2-10s)");
	generateButton.setTooltip("Generate audio loop for selected track (Right-click for MIDI learn)");
	configButton.setTooltip("Configure API settings and generation mode");
	drumsButton.setTooltip("Include drums stem in generation");
	bassButton.setTooltip("Include bass stem in generation");
	otherButton.setTooltip("Include other instruments stem in generation");
	vocalsButton.setTooltip("Include vocals stem in generation");
	guitarButton.setTooltip("Include guitar stem in generation");
	pianoButton.setTooltip("Include piano stem in generation");
	autoLoadButton.setTooltip("Automatically load generated samples (disable for manual control)");
	loadSampleButton.setTooltip("Manually load pending generated sample");
	addTrackButton.setTooltip("Add a new track to the session");
	saveSessionButton.setTooltip("Save current session to file");
	loadSessionButton.setTooltip("Load a previously saved session");
	resetUIButton.setTooltip("Reset UI if stuck in generation mode");
}

void DjIaVstEditor::addEventListeners()
{
	addTrackButton.onClick = [this]()
		{ onAddTrack(); };
	saveSessionButton.onClick = [this]()
		{ onSaveSession(); };
	loadSessionButton.onClick = [this]()
		{ onLoadSession(); };
	autoLoadButton.onClick = [this]
		{ onAutoLoadToggled(); };
	loadSampleButton.onClick = [this]
		{ onLoadSampleClicked(); };
	savePresetButton.onClick = [this]
		{ onSavePreset(); };
	promptPresetSelector.onChange = [this]
		{ onPresetSelected(); };
	promptPresetSelector.addMouseListener(this, false);

	promptInput.onTextChange = [this]()
		{
			audioProcessor.setLastPrompt(promptInput.getText());
			audioProcessor.setGlobalPrompt(promptInput.getText());
		};

	keySelector.onChange = [this]()
		{
			audioProcessor.setLastKeyIndex(keySelector.getSelectedId());
			audioProcessor.setGlobalKey(keySelector.getText());
		};

	durationSlider.onValueChange = [this]()
		{
			audioProcessor.setLastDuration(durationSlider.getValue());
			audioProcessor.setGlobalDuration((int)durationSlider.getValue());
		};

	drumsButton.onClick = [this]()
		{
			audioProcessor.updateGlobalStem("drums", drumsButton.getToggleState());
		};

	bassButton.onClick = [this]()
		{
			audioProcessor.updateGlobalStem("bass", bassButton.getToggleState());
		};

	otherButton.onClick = [this]()
		{
			audioProcessor.updateGlobalStem("other", otherButton.getToggleState());
		};

	vocalsButton.onClick = [this]()
		{
			audioProcessor.updateGlobalStem("vocals", vocalsButton.getToggleState());
		};

	guitarButton.onClick = [this]()
		{
			audioProcessor.updateGlobalStem("guitar", guitarButton.getToggleState());
		};

	pianoButton.onClick = [this]()
		{
			audioProcessor.updateGlobalStem("piano", pianoButton.getToggleState());
		};

	promptPresetSelector.onChange = [this]()
		{
			onPresetSelected();
			audioProcessor.setLastPresetIndex(promptPresetSelector.getSelectedId() - 1);
		};

	resetUIButton.onClick = [this]()
		{
			audioProcessor.setIsGenerating(false);
			audioProcessor.setGeneratingTrackId("");
			generateButton.setEnabled(true);
			setAllGenerateButtonsEnabled(true);
			toggleWaveFormButtonOnTrack();
			toggleSEQButtonOnTrack();
			statusLabel.setText("UI Reset - Ready", juce::dontSendNotification);
			for (auto& trackComp : trackComponents)
			{
				trackComp->stopGeneratingAnimation();
			}
			refreshTracks();
		};

	promptPresetSelector.onMidiLearn = [this]()
		{
			audioProcessor.getMidiLearnManager().startLearning(
				"promptPresetSelector",
				&audioProcessor,
				[this](float value)
				{
					juce::MessageManager::callAsync([this, value]()
						{
							int numItems = promptPresetSelector.getNumItems();
							if (numItems > 0) {
								int selectedIndex = (int)(value * (numItems - 1));
								promptPresetSelector.setSelectedItemIndex(selectedIndex, juce::sendNotification);
							} });
				},
				"Prompt Preset Selector");
		};

	promptPresetSelector.onMidiRemove = [this]()
		{
			audioProcessor.getMidiLearnManager().removeMappingForParameter("promptPresetSelector");
		};

	audioProcessor.getMidiLearnManager().registerUICallback("promptPresetSelector",
		[this](float value)
		{
			juce::MessageManager::callAsync([this, value]()
				{
					int numItems = promptPresetSelector.getNumItems();
					if (numItems > 0) {
						int selectedIndex = (int)(value * (numItems - 1));
						promptPresetSelector.setSelectedItemIndex(selectedIndex, juce::sendNotification);
					} });
		});

	bypassSequencerButton.onClick = [this]()
		{
			bool isBypassed = bypassSequencerButton.getToggleState();
			audioProcessor.setBypassSequencer(isBypassed);

			if (isBypassed)
			{
				statusLabel.setText("Composition mode - Direct MIDI playback", juce::dontSendNotification);
			}
			else
			{
				statusLabel.setText("Sequencer mode - Armed playback", juce::dontSendNotification);
			}
		};

	nextTrackButton.onMidiLearn = [this]()
		{
			audioProcessor.getMidiLearnManager().startLearning(
				"nextTrack",
				&audioProcessor,
				nullptr,
				"Next Track"
			);
		};

	nextTrackButton.onMidiRemove = [this]()
		{
			audioProcessor.getMidiLearnManager().removeMappingForParameter("nextTrack");
		};

	nextTrackButton.onClick = [this]()
		{
			audioProcessor.selectNextTrack();
		};

	prevTrackButton.onMidiLearn = [this]()
		{
			audioProcessor.getMidiLearnManager().startLearning(
				"prevTrack",
				&audioProcessor,
				nullptr,
				"Previous Track"
			);
		};

	prevTrackButton.onMidiRemove = [this]()
		{
			audioProcessor.getMidiLearnManager().removeMappingForParameter("prevTrack");
		};

	prevTrackButton.onClick = [this]()
		{
			audioProcessor.selectPreviousTrack();
		};

	generateButton.onMidiLearn = [this]()
		{
			audioProcessor.getMidiLearnManager().startLearning(
				"generate",
				&audioProcessor,
				nullptr,
				"Generate Loop"
			);
		};

	generateButton.onMidiRemove = [this]()
		{
			audioProcessor.getMidiLearnManager().removeMappingForParameter("generate");
		};

	generateButton.onClick = [this]()
		{
			onGenerateButtonClicked();
		};

	showSampleBankButton.onClick = [this]() { toggleSampleBank(); };

	sampleBankPanel->onSampleDroppedToTrack = [this](const juce::String& sampleId, const juce::String& trackId)
		{
			audioProcessor.loadSampleFromBank(sampleId, trackId);
			setStatusWithTimeout("Sample loaded from bank: " + sampleId.substring(0, 8) + "...", 3000);
		};
}

void DjIaVstEditor::notifyTracksPromptUpdate()
{
	juce::StringArray allPrompts = promptPresets;
	auto customPrompts = audioProcessor.getCustomPrompts();

	for (const auto& customPrompt : customPrompts)
	{
		if (!allPrompts.contains(customPrompt))
		{
			allPrompts.add(customPrompt);
		}
	}
	allPrompts.sort(true);
	for (auto& trackComp : trackComponents)
	{
		trackComp->updatePromptPresets(allPrompts);
	}
}

void DjIaVstEditor::mouseDown(const juce::MouseEvent& event)
{
	if (event.eventComponent == &promptPresetSelector && event.mods.isPopupMenu())
	{
		juce::String selectedPrompt = promptPresetSelector.getText();
		auto customPrompts = audioProcessor.getCustomPrompts();

		if (event.mods.isCtrlDown() && customPrompts.contains(selectedPrompt))
		{
			juce::PopupMenu menu;
			menu.addItem(1, "Edit");
			menu.addItem(2, "Delete");

			menu.showMenuAsync(juce::PopupMenu::Options(), [this, selectedPrompt](int result)
				{
					if (result == 1) {
						editCustomPromptDialog(selectedPrompt);
					}
					else if (result == 2) {
						juce::MessageManager::callAsync([this, selectedPrompt]()
							{
								juce::AlertWindow::showAsync(
									juce::MessageBoxOptions()
									.withIconType(juce::MessageBoxIconType::WarningIcon)
									.withTitle("Delete Custom Prompt")
									.withMessage("Are you sure you want to delete this prompt?\n\n'" + selectedPrompt + "'")
									.withButton("Delete")
									.withButton("Cancel"),
									[this, selectedPrompt](int result)
									{
										if (result == 1)
										{
											audioProcessor.removeCustomPrompt(selectedPrompt);
											promptPresets.removeString(selectedPrompt);
											audioProcessor.setLastPresetIndex(audioProcessor.getLastPresetIndex() - 1);
											loadPromptPresets();
											notifyTracksPromptUpdate();
										}
									});
							});

					} });
		}
	}
}

void DjIaVstEditor::editCustomPromptDialog(const juce::String& selectedPrompt)
{
	auto alertWindow = std::make_unique<juce::AlertWindow>(
		"Edit Custom Prompt",
		"Edit your prompt:",
		juce::MessageBoxIconType::InfoIcon);

	alertWindow->addTextEditor("promptText", selectedPrompt, "Prompt text:");
	alertWindow->addButton("Save", 1);
	alertWindow->addButton("Cancel", 0);

	auto* windowPtr = alertWindow.get();
	alertWindow.release()->enterModalState(true,
		juce::ModalCallbackFunction::create([this, windowPtr, selectedPrompt](int result)
			{
				if (result == 1) {
					auto* promptEditor = windowPtr->getTextEditor("promptText");
					if (promptEditor) {
						juce::String newPrompt = promptEditor->getText();
						if (!newPrompt.isEmpty()) {
							audioProcessor.editCustomPrompt(selectedPrompt, newPrompt);
							int index = promptPresets.indexOf(selectedPrompt);
							if (index >= 0) {
								promptPresets.set(index, newPrompt);
							}
							loadPromptPresets();
						}
					}
				}
				windowPtr->exitModalState(result);
				delete windowPtr; }));
}

void DjIaVstEditor::updateUIFromProcessor()
{
	serverUrlInput.setText(audioProcessor.getServerUrl(), juce::dontSendNotification);
	apiKeyInput.setText(audioProcessor.getApiKey(), juce::dontSendNotification);

	promptInput.setText(audioProcessor.getGlobalPrompt(), juce::dontSendNotification);
	durationSlider.setValue(audioProcessor.getGlobalDuration(), juce::dontSendNotification);

	keySelector.setText(audioProcessor.getGlobalKey(), juce::dontSendNotification);

	drumsButton.setToggleState(audioProcessor.isGlobalStemEnabled("drums"), juce::dontSendNotification);
	bassButton.setToggleState(audioProcessor.isGlobalStemEnabled("bass"), juce::dontSendNotification);
	otherButton.setToggleState(audioProcessor.isGlobalStemEnabled("other"), juce::dontSendNotification);
	vocalsButton.setToggleState(audioProcessor.isGlobalStemEnabled("vocals"), juce::dontSendNotification);
	guitarButton.setToggleState(audioProcessor.isGlobalStemEnabled("guitar"), juce::dontSendNotification);
	pianoButton.setToggleState(audioProcessor.isGlobalStemEnabled("piano"), juce::dontSendNotification);

	autoLoadButton.setToggleState(audioProcessor.getAutoLoadEnabled(), juce::dontSendNotification);
	loadSampleButton.setEnabled(!audioProcessor.getAutoLoadEnabled());

	bypassSequencerButton.setToggleState(audioProcessor.getBypassSequencer(), juce::dontSendNotification);

	int presetIndex = audioProcessor.getLastPresetIndex();
	if (presetIndex >= 0 && presetIndex < promptPresets.size())
	{
		promptPresetSelector.setSelectedId(presetIndex + 1, juce::dontSendNotification);
	}
	else
	{
		promptPresetSelector.setSelectedId(promptPresets.size(), juce::dontSendNotification);
	}

	refreshTrackComponents();
}

void DjIaVstEditor::paint(juce::Graphics& g)
{
	auto bounds = getLocalBounds();
	juce::ColourGradient gradient(
		ColourPalette::backgroundDeep, 0.0f, 0.0f,
		ColourPalette::backgroundMid, 0.0f, static_cast<float>(getHeight()),
		false);
	g.setGradientFill(gradient);
	g.fillAll();

	if (bannerImage.isValid())
	{
		int sourceWidth = bannerImage.getWidth();
		int sourceHeight = (int)((bannerImage.getHeight() - 300) * 0.1f);
		auto sourceArea = juce::Rectangle<int>(0, 0, sourceWidth, sourceHeight);
		juce::Path roundedRect;
		roundedRect.addRoundedRectangle(bannerArea.toFloat(), 6.0f);
		g.saveState();
		g.reduceClipRegion(roundedRect);
		g.drawImage(bannerImage,
			bannerArea.getX(), bannerArea.getY(),
			bannerArea.getWidth(), bannerArea.getHeight(),
			0, 0,
			sourceWidth, sourceHeight,
			false);
		g.restoreState();
	}

	if (logoImage.isValid())
	{
		auto logoArea = juce::Rectangle<int>(0, 40, 100, 60);
		g.drawImage(logoImage, logoArea.toFloat(),
			juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
	}
}

void DjIaVstEditor::layoutPromptSection(juce::Rectangle<int> area, int spacing)
{
	auto row1 = area.removeFromTop(35);
	int saveButtonWidth = 50;
	promptPresetSelector.setBounds(row1.removeFromLeft(area.getWidth() - saveButtonWidth - spacing));
	row1.removeFromLeft(spacing);
	savePresetButton.setBounds(row1.removeFromLeft(saveButtonWidth));

	area.removeFromTop(spacing);

	auto row2 = area.removeFromTop(35);
	promptInput.setBounds(row2.removeFromLeft(area.getWidth()));
}

void DjIaVstEditor::layoutConfigSection(juce::Rectangle<int> area, int reducing)
{
	auto controlRow = area.removeFromTop(35);
	auto controlWidth = controlRow.getWidth() / 2;

	keySelector.setBounds(controlRow.removeFromLeft(controlWidth).reduced(reducing));
	durationSlider.setBounds(controlRow.removeFromLeft(controlWidth).reduced(reducing));

	auto stemsRow = area.removeFromTop(30);
	auto stemsSection = stemsRow.removeFromLeft(600);
	stemsLabel.setBounds(stemsSection.removeFromLeft(60));
	auto stemsArea = stemsSection.reduced(reducing);
	auto stemWidth = stemsArea.getWidth() / 6;
	drumsButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(reducing));
	bassButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(reducing));
	vocalsButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(reducing));
	guitarButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(reducing));
	pianoButton.setBounds(stemsArea.removeFromLeft(stemWidth).reduced(reducing));
	otherButton.setBounds(stemsArea.reduced(reducing));
}

void DjIaVstEditor::resized()
{
	static bool resizing = false;

	const int spacing = 5;
	const int padding = 10;
	const int reducing = 2;

	if (resizing)
		return;

	resizing = true;

	auto bottomArea = getLocalBounds().removeFromBottom(45);
	auto area = getLocalBounds();

	if (menuBar)
	{
		menuBar->setBounds(area.removeFromTop(24));
	}

	area = area.reduced(padding);
	auto configArea = area.removeFromTop(70);

	this->bannerArea = configArea;

	auto logoSpace = configArea.removeFromLeft(80);
	auto nameArea = configArea.removeFromLeft(300);
	auto titleArea = nameArea.removeFromTop(30);
	auto devArea = nameArea.removeFromTop(10);
	auto partnerArea = nameArea.removeFromTop(25);
	pluginNameLabel.setBounds(titleArea);
	developerLabel.setBounds(devArea);
	stabilityLabel.setBounds(partnerArea);

	auto configButtonArea = configArea.removeFromRight(100);
	configButton.setBounds(configButtonArea.reduced(16));

	area = area.reduced(padding);

	auto promptAndConfigArea = area.removeFromTop(80);
	auto leftSection = promptAndConfigArea.removeFromLeft(promptAndConfigArea.getWidth() / 2);
	promptAndConfigArea.removeFromLeft(20);
	auto rightSection = promptAndConfigArea;

	layoutPromptSection(rightSection, spacing);
	layoutConfigSection(leftSection, reducing);

	area.removeFromTop(spacing);
	auto tracksAndMixerArea = area.removeFromTop(area.getHeight() - 70);
	int tracksWidth = static_cast<int>(tracksAndMixerArea.getWidth() * 0.6f);
	auto tracksMainArea = tracksAndMixerArea.removeFromLeft(tracksWidth);
	tracksViewport.setBounds(tracksMainArea);

	tracksAndMixerArea.removeFromLeft(5);
	if (mixerPanel)
	{
		mixerPanel->setBounds(tracksAndMixerArea);
		mixerPanel->setVisible(true);
	}

	auto buttonsRow = area.removeFromTop(35);
	auto buttonWidth = buttonsRow.getWidth() / 9;
	autoLoadButton.setBounds(buttonsRow.removeFromLeft(buttonWidth).reduced(5));
	bypassSequencerButton.setBounds(buttonsRow.removeFromLeft(buttonWidth).reduced(5));
	addTrackButton.setBounds(buttonsRow.removeFromLeft(buttonWidth).reduced(5));
	generateButton.setBounds(buttonsRow.removeFromLeft(buttonWidth).reduced(5));
	loadSampleButton.setBounds(buttonsRow.removeFromLeft(buttonWidth).reduced(5));
	showSampleBankButton.setBounds(buttonsRow.removeFromLeft(buttonWidth).reduced(5));
	auto trackNavArea = buttonsRow.removeFromRight(buttonWidth * 2);
	prevTrackButton.setBounds(trackNavArea.removeFromLeft(buttonWidth).reduced(5));
	nextTrackButton.setBounds(trackNavArea.reduced(5));
	resetUIButton.setBounds(buttonsRow.reduced(5));

	bottomArea.removeFromTop(spacing);
	statusLabel.setBounds(bottomArea.removeFromTop(20));
	midiIndicator.setBounds(bottomArea.removeFromTop(20));
	if (sampleBankPanel && sampleBankVisible)
	{
		auto bankArea = getLocalBounds().removeFromRight(400).reduced(5);
		sampleBankPanel->setBounds(bankArea);
	}

	resizing = false;
}

void DjIaVstEditor::setAllGenerateButtonsEnabled(bool enabled)
{
	for (auto& trackComp : trackComponents)
	{
		trackComp->setGenerateButtonEnabled(enabled);
	}
}

void DjIaVstEditor::toggleSampleBank()
{
	sampleBankVisible = !sampleBankVisible;
	sampleBankPanel->setVisible(sampleBankVisible);

	if (sampleBankVisible)
	{
		showSampleBankButton.setButtonText("Hide Bank");
		showSampleBankButton.setColour(juce::TextButton::buttonColourId, ColourPalette::coral);
		setStatusWithTimeout("Sample bank opened", 2000);
	}
	else
	{
		showSampleBankButton.setButtonText("Bank");
		showSampleBankButton.setColour(juce::TextButton::buttonColourId, ColourPalette::indigo);
		setStatusWithTimeout("Sample bank closed", 2000);
	}

	resized();
}

void DjIaVstEditor::startGenerationUI(const juce::String& trackId)
{
	generateButton.setEnabled(false);
	setAllGenerateButtonsEnabled(false);
	statusLabel.setText("Connecting to server...", juce::dontSendNotification);

	for (auto& trackComp : trackComponents)
	{
		if (trackComp->getTrackId() == trackId)
		{
			trackComp->startGeneratingAnimation();
			break;
		}
	}
	if (mixerPanel)
	{
		mixerPanel->startGeneratingAnimationForTrack(trackId);
	}
	juce::Timer::callAfterDelay(100, [this]()
		{ statusLabel.setText("Generating loop (this may take a few minutes)...",
			juce::dontSendNotification); });
}

void DjIaVstEditor::stopGenerationUI(const juce::String& trackId, bool success, const juce::String& errorMessage)
{
	generateButton.setEnabled(true);
	setAllGenerateButtonsEnabled(true);

	for (auto& trackComp : trackComponents)
	{
		if (trackComp->getTrackId() == trackId)
		{
			trackComp->stopGeneratingAnimation();
			if (success)
			{
				trackComp->updateFromTrackData();
				trackComp->repaint();
			}
			break;
		}
	}
	if (mixerPanel)
	{
		mixerPanel->stopGeneratingAnimationForTrack(trackId);
	}
	isGenerating.store(false);
	wasGenerating.store(false);
	stopGenerationButtonAnimation();
	stopTimer();
	if (!success && !errorMessage.isEmpty())
	{
		statusLabel.setText("Error: " + errorMessage, juce::dontSendNotification);
	}
}

void DjIaVstEditor::onGenerateButtonClicked()
{
	audioProcessor.syncSelectedTrackWithGlobalPrompt();
	audioProcessor.setIsGenerating(true);
	juce::String serverUrl = audioProcessor.getServerUrl();
	juce::String apiKey = audioProcessor.getApiKey();
	if (serverUrl.isEmpty())
	{
		statusLabel.setText("Error: Server URL is required", juce::dontSendNotification);
		return;
	}
	bool isLocalServer = serverUrl.contains("localhost") ||
		serverUrl.contains("127.0.0.1");
	if (apiKey.isEmpty() && !isLocalServer)
	{
		statusLabel.setText("Error: API Key is required", juce::dontSendNotification);
		return;
	}
	if (promptInput.getText().isEmpty())
	{
		statusLabel.setText("Error: Prompt is required", juce::dontSendNotification);
		return;
	}

	generatingTrackId = audioProcessor.getSelectedTrackId();
	audioProcessor.setGeneratingTrackId(generatingTrackId);
	TrackData* track = audioProcessor.trackManager.getTrack(generatingTrackId);

	if (!track)
	{
		statusLabel.setText("Error: No track selected", juce::dontSendNotification);
		return;
	}

	if (track->usePages.load()) {
		auto& targetPage = track->pages[track->displayPageIndex];
		targetPage.selectedPrompt = promptInput.getText();
		targetPage.generationPrompt = promptInput.getText();
		targetPage.generationBpm = (float)audioProcessor.getHostBpm();
		targetPage.generationKey = keySelector.getText();
		targetPage.generationDuration = (int)durationSlider.getValue();
		targetPage.preferredStems.clear();
		if (drumsButton.getToggleState())
			targetPage.preferredStems.push_back("drums");
		if (bassButton.getToggleState())
			targetPage.preferredStems.push_back("bass");
		if (otherButton.getToggleState())
			targetPage.preferredStems.push_back("other");
		if (vocalsButton.getToggleState())
			targetPage.preferredStems.push_back("vocals");
		if (guitarButton.getToggleState())
			targetPage.preferredStems.push_back("guitar");
		if (pianoButton.getToggleState())
			targetPage.preferredStems.push_back("piano");
		track->syncLegacyProperties();

		DBG("Global generation for page " << (char)('A' + track->displayPageIndex) <<
			" - Prompt: " << targetPage.selectedPrompt);
	}
	else {
		track->generationPrompt = promptInput.getText();
		track->generationBpm = (float)audioProcessor.getHostBpm();
		track->generationKey = keySelector.getText();
		track->generationDuration = (int)durationSlider.getValue();
		track->selectedPrompt.clear();
		track->preferredStems.clear();
		if (drumsButton.getToggleState())
			track->preferredStems.push_back("drums");
		if (bassButton.getToggleState())
			track->preferredStems.push_back("bass");
		if (otherButton.getToggleState())
			track->preferredStems.push_back("other");
		if (vocalsButton.getToggleState())
			track->preferredStems.push_back("vocals");
		if (guitarButton.getToggleState())
			track->preferredStems.push_back("guitar");
		if (pianoButton.getToggleState())
			track->preferredStems.push_back("piano");
	}

	startGenerationUI(generatingTrackId);
	juce::String selectedTrackId = generatingTrackId;
	auto request = track->createLoopRequestForDisplayPage(track->displayPageIndex);
	juce::Thread::launch([this, selectedTrackId, request]()
		{
			try
			{
				juce::MessageManager::callAsync([this]() {
					statusLabel.setText("Generating loop (this may take a few minutes)...",
						juce::dontSendNotification);
					});

				audioProcessor.setServerUrl(audioProcessor.getServerUrl());
				audioProcessor.setApiKey(audioProcessor.getApiKey());
				juce::Thread::sleep(100);
				audioProcessor.generateLoop(request, generatingTrackId);
			}
			catch (const std::exception& e)
			{
				juce::MessageManager::callAsync([this, selectedTrackId, error = juce::String(e.what())]() {
					stopGenerationUI(selectedTrackId, false, error);
					audioProcessor.setIsGenerating(false);
					audioProcessor.setGeneratingTrackId("");
					});
			} });
}

void DjIaVstEditor::loadPromptPresets()
{
	promptPresetSelector.clear();
	juce::StringArray allPrompts = promptPresets;
	auto customPrompts = audioProcessor.getCustomPrompts();
	for (const auto& customPrompt : customPrompts)
	{
		if (!allPrompts.contains(customPrompt))
		{
			allPrompts.add(customPrompt);
		}
	}
	allPrompts.sort(true);
	promptPresets = allPrompts;
	for (int i = 0; i < allPrompts.size(); ++i)
	{
		promptPresetSelector.addItem(allPrompts[i], i + 1);
	}
	int lastPresetIndex = audioProcessor.getLastPresetIndex();
	if (lastPresetIndex >= 1 && lastPresetIndex <= allPrompts.size())
	{
		promptPresetSelector.setSelectedId(lastPresetIndex + 1, juce::dontSendNotification);
	}
	else
	{
		promptPresetSelector.setSelectedId(1, juce::dontSendNotification);
	}
	juce::String selectedPresetText = promptPresetSelector.getText();
	promptInput.setText(selectedPresetText, juce::dontSendNotification);
}

void DjIaVstEditor::onPresetSelected()
{
	int selectedId = promptPresetSelector.getSelectedId();
	audioProcessor.setLastPresetIndex(selectedId);
	juce::String selectedPrompt = promptPresetSelector.getText();
	if (!selectedPrompt.isEmpty())
	{
		promptInput.setText(selectedPrompt);
		statusLabel.setText("Preset loaded: " + selectedPrompt, juce::dontSendNotification);
	}
	else
	{
		promptInput.clear();
		statusLabel.setText("Custom prompt mode", juce::dontSendNotification);
	}
}

void DjIaVstEditor::onSavePreset()
{
	juce::String currentPrompt = promptInput.getText().trim();
	if (currentPrompt.isNotEmpty())
	{
		audioProcessor.addCustomPrompt(currentPrompt);
		loadPromptPresets();
		notifyTracksPromptUpdate();
		int totalItems = promptPresetSelector.getNumItems();
		for (int i = 0; i < totalItems; ++i)
		{
			if (promptPresetSelector.getItemText(i) == currentPrompt)
			{
				promptPresetSelector.setSelectedId(i + 1, juce::dontSendNotification);
				break;
			}
		}

		statusLabel.setText("Preset saved: " + currentPrompt, juce::dontSendNotification);
	}
	else
	{
		statusLabel.setText("Enter a prompt first!", juce::dontSendNotification);
	}
}

void DjIaVstEditor::onAutoLoadToggled()
{
	bool autoLoadOn = autoLoadButton.getToggleState();
	audioProcessor.setAutoLoadEnabled(autoLoadOn);

	if (autoLoadOn)
	{
		statusLabel.setText("Auto-load enabled - samples load automatically", juce::dontSendNotification);
		loadSampleButton.setButtonText("Load Sample");
		loadSampleButton.setEnabled(false);
	}
	else
	{
		statusLabel.setText("Manual mode - click Load Sample when ready", juce::dontSendNotification);
		loadSampleButton.setEnabled(true);
		updateLoadButtonState();
	}
}

void DjIaVstEditor::onLoadSampleClicked()
{
	if (audioProcessor.hasSampleWaiting())
	{
		audioProcessor.loadPendingSample();
		statusLabel.setText("Sample loaded manually!", juce::dontSendNotification);
		updateLoadButtonState();
	}
	else
	{
		statusLabel.setText("Generate a loop first", juce::dontSendNotification);
	}
}

void DjIaVstEditor::updateLoadButtonState()
{
	if (!autoLoadButton.getToggleState())
	{
		if (audioProcessor.hasSampleWaiting())
		{
			loadSampleButton.setButtonText("Load Sample (Ready!)");
			loadSampleButton.setColour(juce::TextButton::buttonColourId, ColourPalette::amber);
		}
		else
		{
			loadSampleButton.setButtonText("Load Sample");
			loadSampleButton.setColour(juce::TextButton::buttonColourId, ColourPalette::coral);
		}
	}
}

void DjIaVstEditor::refreshTrackComponents()
{
	auto trackIds = audioProcessor.getAllTrackIds();
	std::sort(trackIds.begin(), trackIds.end(),
		[this](const juce::String& a, const juce::String& b)
		{
			TrackData* trackA = audioProcessor.getTrack(a);
			TrackData* trackB = audioProcessor.getTrack(b);
			if (!trackA || !trackB)
				return false;

			return trackA->slotIndex < trackB->slotIndex;
		});
	if (trackComponents.size() == trackIds.size())
	{
		bool allVisible = true;
		for (auto& comp : trackComponents)
		{
			if (!comp->isVisible() || comp->getParentComponent() == nullptr)
			{
				allVisible = false;
				break;
			}
		}
		if (allVisible)
		{
			for (int i = 0; i < trackComponents.size() && i < trackIds.size(); ++i)
			{
				trackComponents[i]->setTrackData(audioProcessor.getTrack(trackIds[i]));

				juce::Timer::callAfterDelay(100, [this, i]()
					{ trackComponents[i]->updatePromptPresets(getAllPrompts()); });
				trackComponents[i]->updateFromTrackData();
				if (auto* sequencer = trackComponents[i]->getSequencer())
				{
					sequencer->updateFromTrackData();
				}
			}
			updateSelectedTrack();
			return;
		}
	}

	setEnabled(false);
	juce::String previousSelectedId = audioProcessor.getSelectedTrackId();

	trackComponents.clear();
	tracksContainer.removeAllChildren();
	int yPos = 5;

	for (const auto& trackId : trackIds)
	{
		TrackData* trackData = audioProcessor.getTrack(trackId);
		if (!trackData)
			continue;

		auto trackComp = std::make_unique<TrackComponent>(trackId, audioProcessor);
		trackComp->setTrackData(trackData);
		TrackComponent* trackCompPtr = trackComp.get();
		juce::Timer::callAfterDelay(100, [this, trackCompPtr, trackId]()
			{
				auto it = std::find_if(trackComponents.begin(), trackComponents.end(),
					[trackCompPtr](const auto& tc) { return tc.get() == trackCompPtr; });

				if (it != trackComponents.end() && trackCompPtr->getTrack() && !trackCompPtr->getTrack()->selectedPrompt.isEmpty())
				{
					trackCompPtr->updatePromptPresets(getAllPrompts());
				} });
				trackComp->onSelectTrack = [this](const juce::String& id)
					{
						audioProcessor.selectTrack(id);
						updateSelectedTrack();
					};

				trackComp->onDeleteTrack = [this](const juce::String& id)
					{
						if (audioProcessor.getAllTrackIds().size() > 1)
						{
							audioProcessor.deleteTrack(id);
							juce::Timer::callAfterDelay(10, [this]()
								{ refreshTrackComponents(); });
						}
					};

				trackComp->onTrackRenamed = [this](const juce::String& id, const juce::String& newName)
					{
						if (mixerPanel)
						{
							mixerPanel->updateTrackName(id, newName);
						}
					};

				trackComp->onGenerateForTrack = [this](const juce::String& id)
					{
						audioProcessor.selectTrack(id);
						generateFromTrackComponent(id);
					};

				trackComp->onReorderTrack = [this](const juce::String& fromId, const juce::String& toId)
					{
						audioProcessor.reorderTracks(fromId, toId);
						juce::Timer::callAfterDelay(10, [this]()
							{ refreshTrackComponents(); });
					};

				trackComp->onPreviewTrack = [this](const juce::String& trackId)
					{
						audioProcessor.previewTrack(trackId);
					};

				trackComp->onTrackPromptChanged = [this](const juce::String /*&trackId*/, const juce::String& prompt)
					{
						setStatusWithTimeout("Track prompt updated: " + prompt.substring(0, 20) + "...", 3000);
					};

				trackComp->onStatusMessage = [this](const juce::String& message)
					{
						setStatusWithTimeout(message, 3000);
					};

				int fullWidth = tracksContainer.getWidth() - 4;
				trackComp->setBounds(2, yPos, fullWidth, 60);

				if (trackId == audioProcessor.getSelectedTrackId())
				{
					trackComp->setSelected(true);
				}
				tracksContainer.addAndMakeVisible(trackComp.get());
				trackComponents.push_back(std::move(trackComp));

				yPos += 85;
	}

	tracksContainer.setSize(tracksViewport.getWidth() - 20, yPos + 5);
	if (mixerPanel)
	{
		mixerPanel->refreshMixerChannels();
	}

	setEnabled(true);
	juce::MessageManager::callAsync([this]()
		{
			resized();
			repaint(); });
	tracksContainer.repaint();
}

void DjIaVstEditor::generateFromTrackComponent(const juce::String& trackId)
{
	audioProcessor.setIsGenerating(true);

	TrackData* track = audioProcessor.getTrack(trackId);
	if (!track)
	{
		statusLabel.setText("Error: Track not found", juce::dontSendNotification);
		audioProcessor.setIsGenerating(false);
		return;
	}

	if (track->selectedPrompt.isEmpty())
	{
		statusLabel.setText("Error: No prompt selected for this track", juce::dontSendNotification);
		audioProcessor.setIsGenerating(false);
		return;
	}

	juce::String currentGeneratingTrackId = trackId;
	audioProcessor.setGeneratingTrackId(currentGeneratingTrackId);

	if (track->usePages.load()) {
		auto& targetPage = track->pages[track->displayPageIndex];

		targetPage.selectedPrompt = track->selectedPrompt;
		targetPage.generationPrompt = track->selectedPrompt;
		targetPage.generationBpm = audioProcessor.getGlobalBpm();
		targetPage.generationKey = audioProcessor.getGlobalKey();
		targetPage.generationDuration = audioProcessor.getGlobalDuration();

		targetPage.preferredStems.clear();
		if (audioProcessor.isGlobalStemEnabled("drums"))
			targetPage.preferredStems.push_back("drums");
		if (audioProcessor.isGlobalStemEnabled("bass"))
			targetPage.preferredStems.push_back("bass");
		if (audioProcessor.isGlobalStemEnabled("other"))
			targetPage.preferredStems.push_back("other");
		if (audioProcessor.isGlobalStemEnabled("vocals"))
			targetPage.preferredStems.push_back("vocals");
		if (audioProcessor.isGlobalStemEnabled("guitar"))
			targetPage.preferredStems.push_back("guitar");
		if (audioProcessor.isGlobalStemEnabled("piano"))
			targetPage.preferredStems.push_back("piano");


		DBG("Track generation for page " << (char)('A' + track->displayPageIndex) <<
			" - Prompt: " << targetPage.selectedPrompt);
	}
	else {
		track->generationBpm = audioProcessor.getGlobalBpm();
		track->generationKey = audioProcessor.getGlobalKey();
		track->generationDuration = audioProcessor.getGlobalDuration();

		track->preferredStems.clear();
		if (audioProcessor.isGlobalStemEnabled("drums"))
			track->preferredStems.push_back("drums");
		if (audioProcessor.isGlobalStemEnabled("bass"))
			track->preferredStems.push_back("bass");
		if (audioProcessor.isGlobalStemEnabled("other"))
			track->preferredStems.push_back("other");
		if (audioProcessor.isGlobalStemEnabled("vocals"))
			track->preferredStems.push_back("vocals");
		if (audioProcessor.isGlobalStemEnabled("guitar"))
			track->preferredStems.push_back("guitar");
		if (audioProcessor.isGlobalStemEnabled("piano"))
			track->preferredStems.push_back("piano");
	}

	startGenerationUI(currentGeneratingTrackId);

	juce::Thread::launch([this, currentGeneratingTrackId, track]()
		{
			try {
				auto request = track->createLoopRequestForDisplayPage(track->displayPageIndex);
				audioProcessor.generateLoop(request, currentGeneratingTrackId);
			}
			catch (const std::exception& e) {
				juce::MessageManager::callAsync([this, currentGeneratingTrackId, error = juce::String(e.what())]() {
					stopGenerationUI(currentGeneratingTrackId, false, error);
					audioProcessor.setIsGenerating(false);
					audioProcessor.setGeneratingTrackId("");
					});
			}
		});
}

juce::StringArray DjIaVstEditor::getAllPrompts() const
{
	juce::StringArray allPrompts = promptPresets;
	auto customPrompts = audioProcessor.getCustomPrompts();

	for (const auto& customPrompt : customPrompts)
	{
		if (!allPrompts.contains(customPrompt))
		{
			allPrompts.add(customPrompt);
		}
	}

	return allPrompts;
}

void DjIaVstEditor::toggleWaveFormButtonOnTrack()
{
	auto trackIds = audioProcessor.getAllTrackIds();
	for (const auto& trackId : trackIds)
	{
		TrackData* track = audioProcessor.getTrack(trackId);
		if (track)
		{
			track->showWaveform = false;
		}
	}
	for (auto& trackComponent : trackComponents)
	{
		trackComponent->showWaveformButton.setToggleState(false, juce::dontSendNotification);
	}
}

void DjIaVstEditor::restoreUICallbacks()
{
	for (auto& trackComp : trackComponents)
	{
		if (trackComp->getTrack())
		{
			trackComp->setupMidiLearn();
		}
	}
}

void DjIaVstEditor::toggleSEQButtonOnTrack()
{
	auto trackIds = audioProcessor.getAllTrackIds();
	for (const auto& trackId : trackIds)
	{
		TrackData* track = audioProcessor.getTrack(trackId);
		if (track)
		{
			track->showSequencer = false;
		}
	}
	for (auto& trackComponent : trackComponents)
	{
		trackComponent->sequencerToggleButton.setToggleState(false, juce::dontSendNotification);
	}
}

void DjIaVstEditor::setStatusWithTimeout(const juce::String& message, int timeoutMs)
{
	statusLabel.setText(message, juce::dontSendNotification);
	juce::Timer::callAfterDelay(timeoutMs, [safeThis = juce::Component::SafePointer<DjIaVstEditor>(this)]()
		{
			if (auto* editor = safeThis.getComponent())
				editor->statusLabel.setText("Ready", juce::dontSendNotification); });
}

void DjIaVstEditor::onAddTrack()
{
	try
	{
		juce::String newTrackId = audioProcessor.createNewTrack();
		refreshTrackComponents();

		if (mixerPanel)
		{
			mixerPanel->trackAdded(newTrackId);
		}
		toggleWaveFormButtonOnTrack();
		toggleSEQButtonOnTrack();
		setStatusWithTimeout("New track created");
	}
	catch (const std::exception& e)
	{
		setStatusWithTimeout("Error: " + juce::String(e.what()));
	}
}

void DjIaVstEditor::updateSelectedTrack()
{
	for (auto& trackComp : trackComponents)
	{
		trackComp->setSelected(false);
	}

	juce::String selectedId = audioProcessor.getSelectedTrackId();

	bool found = false;
	for (auto& trackComp : trackComponents)
	{
		if (trackComp->getTrackId() == selectedId)
		{
			trackComp->setSelected(true);
			found = true;
			break;
		}
	}

	if (mixerPanel)
	{
		mixerPanel->trackSelected(selectedId);
	}
}

void DjIaVstEditor::onSaveSession()
{
	juce::AlertWindow::showAsync(
		juce::MessageBoxOptions()
		.withIconType(juce::MessageBoxIconType::QuestionIcon)
		.withTitle("Save Session")
		.withMessage("Enter session name:")
		.withButton("Save")
		.withButton("Cancel"),
		[this](int result)
		{
			if (result == 1)
			{
				juce::String sessionName = "Session_" + juce::String(juce::Time::getCurrentTime().toMilliseconds());

				auto alertWindow = std::make_unique<juce::AlertWindow>("Save Session",
					"Enter session name:", juce::MessageBoxIconType::QuestionIcon);
				alertWindow->addTextEditor("sessionName", sessionName, "Session name:");
				alertWindow->addButton("Save", 1);
				alertWindow->addButton("Cancel", 0);

				alertWindow->enterModalState(true, juce::ModalCallbackFunction::create([this](int modalResult)
					{
						if (modalResult == 1) {
							auto* nameEditor = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent())
								->getTextEditor("sessionName");
							if (nameEditor) {
								saveCurrentSession(nameEditor->getText());
							}
						} }));
			}
		});
}

void DjIaVstEditor::saveCurrentSession(const juce::String& sessionName)
{
	try
	{
		juce::File sessionsDir = getSessionsDirectory();
		if (!sessionsDir.exists())
		{
			sessionsDir.createDirectory();
		}

		juce::File sessionFile = sessionsDir.getChildFile(sessionName + ".djiasession");

		juce::MemoryBlock stateData;
		audioProcessor.getStateInformation(stateData);

		juce::FileOutputStream stream(sessionFile);
		if (stream.openedOk())
		{
			stream.write(stateData.getData(), stateData.getSize());
			statusLabel.setText("Session saved: " + sessionName, juce::dontSendNotification);
			loadSessionList();
		}
		else
		{
			statusLabel.setText("Failed to save session file", juce::dontSendNotification);
		}
	}
	catch (const std::exception& e)
	{
		statusLabel.setText("Failed to save session: " + juce::String(e.what()),
			juce::dontSendNotification);
	}
}

void DjIaVstEditor::onLoadSession()
{
	int selectedIndex = sessionSelector.getSelectedItemIndex();
	if (selectedIndex >= 0)
	{
		juce::String sessionName = sessionSelector.getItemText(selectedIndex);
		if (sessionName != "No sessions found")
		{
			loadSession(sessionName);
		}
	}
	else
	{
		statusLabel.setText("Please select a session to load", juce::dontSendNotification);
	}
}

void DjIaVstEditor::loadSession(const juce::String& sessionName)
{
	try
	{
		juce::File sessionFile = getSessionsDirectory().getChildFile(sessionName + ".djiasession");

		if (sessionFile.existsAsFile())
		{
			juce::FileInputStream stream(sessionFile);
			if (stream.openedOk())
			{
				juce::MemoryBlock stateData;
				stream.readIntoMemoryBlock(stateData);

				audioProcessor.setStateInformation(stateData.getData(),
					static_cast<int>(stateData.getSize()));

				refreshTrackComponents();
				updateUIFromProcessor();
				statusLabel.setText("Session loaded: " + sessionName, juce::dontSendNotification);
			}
			else
			{
				statusLabel.setText("Failed to read session file", juce::dontSendNotification);
			}
		}
		else
		{
			statusLabel.setText("Session file not found: " + sessionName, juce::dontSendNotification);
		}
	}
	catch (const std::exception& e)
	{
		statusLabel.setText("Failed to load session: " + juce::String(e.what()),
			juce::dontSendNotification);
	}
}

void DjIaVstEditor::loadSessionList()
{
	sessionSelector.clear();

	juce::File sessionsDir = getSessionsDirectory();
	if (sessionsDir.exists())
	{
		auto sessionFiles = sessionsDir.findChildFiles(juce::File::findFiles, false, "*.djiasession");

		for (const auto& file : sessionFiles)
		{
			sessionSelector.addItem(file.getFileNameWithoutExtension(),
				sessionSelector.getNumItems() + 1);
		}
	}

	if (sessionSelector.getNumItems() == 0)
	{
		sessionSelector.addItem("No sessions found", 1);
	}
	else
	{
		sessionSelector.setSelectedItemIndex(0);
	}
}

juce::File DjIaVstEditor::getSessionsDirectory()
{
	return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
		.getChildFile("OBSIDIAN-Neural")
		.getChildFile("Sessions");
}

juce::StringArray DjIaVstEditor::getMenuBarNames()
{
	return { "File", "Tracks", "Help" };
}

juce::PopupMenu DjIaVstEditor::getMenuForIndex(int topLevelMenuIndex, const juce::String& /*menuName*/)
{
	juce::PopupMenu menu;

	if (topLevelMenuIndex == 0)
	{
		menu.addItem(newSession, "New Session", true);
		menu.addSeparator();
		menu.addItem(saveSession, "Save Session", true);
		menu.addItem(saveSessionAs, "Save Session As...", true);
		menu.addItem(loadSessionMenu, "Load Session...", true);
		menu.addSeparator();
		menu.addItem(exportSession, "Export Session", true);
	}
	else if (topLevelMenuIndex == 1)
	{
		menu.addItem(addTrack, "Add New Track", true);
		menu.addSeparator();
		menu.addItem(deleteAllTracks, "Delete All Tracks", audioProcessor.getAllTrackIds().size() > 1);
		menu.addItem(resetTracks, "Reset All Tracks", true);
	}
	else if (topLevelMenuIndex == 2)
	{
		menu.addItem(aboutDjIa, "About OBSIDIAN-Neural", true);
		menu.addItem(showHelp, "Show Help", true);
	}

	return menu;
}

void DjIaVstEditor::menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/)
{
	switch (menuItemID)
	{
	case newSession:
		statusLabel.setText("New session created", juce::dontSendNotification);
		break;

	case saveSession:
		onSaveSession();
		break;

	case saveSessionAs:
		onSaveSession();
		break;

	case loadSessionMenu:
		onLoadSession();
		break;

	case exportSession:
		statusLabel.setText("Export - Coming soon!", juce::dontSendNotification);
		break;

	case addTrack:
		onAddTrack();
		break;

	case deleteAllTracks:
		juce::AlertWindow::showAsync(
			juce::MessageBoxOptions()
			.withIconType(juce::MessageBoxIconType::WarningIcon)
			.withTitle("Delete All Tracks")
			.withMessage("Are you sure you want to delete all tracks?")
			.withButton("Delete")
			.withButton("Cancel"),
			[this](int result)
			{
				if (result == 1)
				{
					auto trackIds = audioProcessor.getAllTrackIds();
					for (int i = 1; i < trackIds.size(); ++i)
					{
						audioProcessor.deleteTrack(trackIds[i]);
					}
					refreshTrackComponents();
					statusLabel.setText("All tracks deleted except one", juce::dontSendNotification);
				}
			});
		break;

	case resetTracks:
		statusLabel.setText("Reset tracks - Coming soon!", juce::dontSendNotification);
		break;

	case aboutDjIa:
		juce::AlertWindow::showAsync(
			juce::MessageBoxOptions()
			.withIconType(juce::MessageBoxIconType::InfoIcon)
			.withTitle("About OBSIDIAN-Neural")
			.withMessage("OBSIDIAN-Neural\n\nVersion: " + Version::FULL +
				"\nBecause writing melodies is hard\n"
				"Let the robots do the work while you take credit\n\n"
				"GitHub: https://github.com/innermost47/ai-dj\n\n"
				"InnerMost47 - Probably overthinking this")
			.withButton("OK"),
			nullptr);
		break;

	case showHelp:
		juce::AlertWindow::showAsync(
			juce::MessageBoxOptions()
			.withIconType(juce::MessageBoxIconType::InfoIcon)
			.withTitle("OBSIDIAN-Neural Help")
			.withMessage("Quick Start:\n"
				"1. Configure server URL and API key\n"
				"2. Add tracks and assign MIDI notes\n"
				"3. Generate loops with prompts\n"
				"4. Play with MIDI keyboard!\n\n"
				"Each track can be triggered by its assigned MIDI note.\n\n"
				"MIDI Learn:\n"
				"- Samples: C3-G3 (notes 60-67) reserved for track triggers\n"
				"- Controls: Use notes 0-59 or 68-127 for MIDI mapping\n"
				"- Assignable controls: Play, Mute, Solo, Pitch, Fine, Pan\n"
				"- Right-click any control, Enable MIDI Learn, Press MIDI control\n\n"
				"Play Button Behavior:\n"
				"- First click: Arms sample to start on next MIDI note\n"
				"- Second click: Arms sample to stop on next MIDI note\n"
				"- Stop button: Arms for immediate stop on next MIDI note\n"
				"- No separate Stop in MIDI Learn - Play button handles both!")
			.withButton("OK"),
			nullptr);
		break;
	}
}

void* DjIaVstEditor::getSequencerForTrack(const juce::String& trackId)
{
	for (auto& trackComp : trackComponents)
	{
		if (trackComp->getTrackId() == trackId)
		{
			return (void*)trackComp->getSequencer();
		}
	}
	return nullptr;
}

void DjIaVstEditor::refreshMixerChannels()
{
	if (mixerPanel)
	{
		mixerPanel->refreshAllChannels();
	}
}
