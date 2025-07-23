/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (C) 2025 Anthony Charretier
 */

#pragma once
#include "JuceHeader.h"

class DjIaVstProcessor;

class SequencerComponent : public juce::Component
{
public:
	SequencerComponent(const juce::String& trackId, DjIaVstProcessor& processor);

	void paint(juce::Graphics& g) override;
	void resized() override;
	void mouseDown(const juce::MouseEvent& event) override;

	void setCurrentStep(int step);
	void setPlaying(bool playing);
	void setNumMeasures(int measures);
	void setCurrentMeasure(int measure);

	void updateFromTrackData();

	bool isSequencerPlaying() const { return isPlaying; }

	int getStepPageAssignment(int measure, int step) const;

private:
	juce::String trackId;
	DjIaVstProcessor& audioProcessor;

	static const int MAX_STEPS_PER_MEASURE = 16;
	static const int MAX_MEASURES = 4;

	juce::Label pageHelpLabel;

	bool isEditing = false;

	int currentStep = 0;
	int currentMeasure = 0;
	int numMeasures = 1;
	int beatsPerMeasure = 4;
	bool isPlaying = false;

	juce::Slider measureSlider;
	juce::Slider timeSignatureSlider;

	juce::Timer* editingTimer = nullptr;

	juce::TextButton prevMeasureButton, nextMeasureButton;

	juce::Label measureLabel;
	juce::Label currentPlayingMeasureLabel;

	std::array<std::array<int, 16>, 4> stepPages;

	juce::Rectangle<int> getStepBounds(int step);

	void toggleStep(int step);
	void setupUI();
	juce::Colour getPageColour(int pageIndex);

	int getTotalStepsForCurrentSignature() const;

	double samplesPerStep;
	double stepAccumulator;
};