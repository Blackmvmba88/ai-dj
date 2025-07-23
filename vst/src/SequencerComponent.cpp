﻿/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (C) 2025 Anthony Charretier
 */

#include "SequencerComponent.h"
#include "PluginProcessor.h"
#include "ColourPalette.h"

SequencerComponent::SequencerComponent(const juce::String& trackId, DjIaVstProcessor& processor)
	: trackId(trackId), audioProcessor(processor)
{
	setupUI();
	updateFromTrackData();
}

void SequencerComponent::setupUI()
{

	addAndMakeVisible(measureSlider);
	measureSlider.setRange(1, 4, 1);
	measureSlider.setValue(1);
	measureSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 30, 20);
	measureSlider.setDoubleClickReturnValue(true, 1);
	measureSlider.setColour(juce::Slider::thumbColourId, ColourPalette::sliderThumb);
	measureSlider.setColour(juce::Slider::trackColourId, ColourPalette::sliderTrack);
	measureSlider.setColour(juce::Slider::backgroundColourId, juce::Colours::black);
	measureSlider.onValueChange = [this]()
		{
			isEditing = true;
			setNumMeasures((int)measureSlider.getValue());
			juce::Timer::callAfterDelay(500, [this]()
				{ isEditing = false; });
		};

	addAndMakeVisible(prevMeasureButton);
	prevMeasureButton.setButtonText("<");
	prevMeasureButton.onClick = [this]()
		{
			isEditing = true;
			if (currentMeasure > 0)
			{
				setCurrentMeasure(currentMeasure - 1);
			}
			juce::Timer::callAfterDelay(500, [this]()
				{ isEditing = false; });
		};

	addAndMakeVisible(nextMeasureButton);
	nextMeasureButton.setButtonText(">");
	nextMeasureButton.onClick = [this]()
		{
			isEditing = true;
			if (currentMeasure < numMeasures - 1)
			{
				setCurrentMeasure(currentMeasure + 1);
			}
			juce::Timer::callAfterDelay(500, [this]()
				{ isEditing = false; });
		};

	addAndMakeVisible(measureLabel);
	measureLabel.setText("1/1", juce::dontSendNotification);
	measureLabel.setJustificationType(juce::Justification::centred);

	addAndMakeVisible(currentPlayingMeasureLabel);
	currentPlayingMeasureLabel.setText("M 1", juce::dontSendNotification);
	currentPlayingMeasureLabel.setColour(juce::Label::textColourId, ColourPalette::textSuccess);
	currentPlayingMeasureLabel.setColour(juce::Label::backgroundColourId, ColourPalette::backgroundDark);
	currentPlayingMeasureLabel.setJustificationType(juce::Justification::centred);
	currentPlayingMeasureLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));

	addAndMakeVisible(pageHelpLabel);
	pageHelpLabel.setText("Click: A>>B>>C>>D>>USER>>Off", juce::dontSendNotification);
	pageHelpLabel.setColour(juce::Label::textColourId, ColourPalette::textSecondary.withAlpha(0.7f));
	pageHelpLabel.setJustificationType(juce::Justification::centredLeft);
	pageHelpLabel.setFont(juce::FontOptions(9.0f, juce::Font::plain));

	for (int m = 0; m < 4; ++m) {
		for (int s = 0; s < 16; ++s) {
			stepPages[m][s] = 0;
		}
	}
}

juce::Colour SequencerComponent::getPageColour(int pageIndex)
{
	switch (pageIndex) {
	case 0: return juce::Colour(0xff81C784);
	case 1: return juce::Colour(0xff64B5F6);
	case 2: return juce::Colour(0xffFFB74D);
	case 3: return juce::Colour(0xffF06292);
	case 4: return juce::Colour(0xffBA68C8);
	default: return ColourPalette::textPrimary;
	}
}
int SequencerComponent::getStepPageAssignment(int measure, int step) const
{
	if (measure >= 0 && measure < MAX_MEASURES && step >= 0 && step < 16) {
		return stepPages[measure][step];
	}
	return 0;
}

void SequencerComponent::paint(juce::Graphics& g)
{
	auto bounds = getLocalBounds();

	float height = static_cast<float>(bounds.getHeight());
	juce::ColourGradient gradient(
		ColourPalette::backgroundDeep, 0.0f, 0.0f,
		ColourPalette::backgroundMid, 0.0f, height,
		false);
	g.setGradientFill(gradient);
	g.fillRoundedRectangle(bounds.toFloat(), 6.0f);

	juce::Colour accentColour = ColourPalette::sequencerAccent;
	juce::Colour beatColour = ColourPalette::sequencerBeat;
	juce::Colour subBeatColour = ColourPalette::sequencerSubBeat;

	TrackData* track = audioProcessor.getTrack(trackId);
	if (!track)
	{
		g.setColour(ColourPalette::textDanger);
		g.drawText("Track not found", getLocalBounds(), juce::Justification::centred);
		return;
	}

	int numerator = audioProcessor.getTimeSignatureNumerator();
	int denominator = audioProcessor.getTimeSignatureDenominator();
	juce::Colour trackColour = ColourPalette::getTrackColour(track->slotIndex);

	int stepsPerBeat;
	if (denominator == 8)
	{
		stepsPerBeat = 4;
	}
	else if (denominator == 4)
	{
		stepsPerBeat = 4;
	}
	else if (denominator == 2)
	{
		stepsPerBeat = 8;
	}
	else
	{
		stepsPerBeat = 4;
	}
	int totalSteps = getTotalStepsForCurrentSignature();
	int playingMeasure = track->sequencerData.currentMeasure;
	int safeMeasure = juce::jlimit(0, MAX_MEASURES - 1, currentMeasure);

	for (int i = 0; i < totalSteps; ++i)
	{
		auto stepBounds = getStepBounds(i);
		bool isVisible = (i < totalSteps);
		bool isStrongBeat = false;
		bool isBeat = false;

		if (isVisible)
		{
			if (denominator == 8)
			{
				if (numerator == 6)
				{
					isStrongBeat = (i % 12 == 0);
					isBeat = (i % 6 == 0);
				}
				else if (numerator == 9)
				{
					isStrongBeat = (i % 12 == 0);
					isBeat = (i % 4 == 0);
				}
				else
				{
					isStrongBeat = (i % (stepsPerBeat * 2) == 0);
					isBeat = (i % stepsPerBeat == 0);
				}
			}
			else
			{
				isStrongBeat = (i % stepsPerBeat == 0);
				isBeat = (i % (stepsPerBeat / 2) == 0);
			}
		}

		juce::Colour stepColour;
		juce::Colour borderColour;

		if (!isVisible)
		{
			stepColour = ColourPalette::backgroundDeep;
			borderColour = ColourPalette::backgroundMid;
		}
		else if (track->sequencerData.steps[safeMeasure][i])
		{
			if (track->usePages.load()) {
				int assignedPage = stepPages[safeMeasure][i];
				stepColour = getPageColour(assignedPage);
				borderColour = stepColour.brighter(0.4f);
			}
			else {
				stepColour = trackColour;
				borderColour = trackColour.brighter(0.4f);
			}
		}
		else
		{
			if (isStrongBeat)
			{
				stepColour = accentColour.withAlpha(0.3f);
				borderColour = accentColour;
			}
			else if (isBeat)
			{
				stepColour = beatColour.withAlpha(0.3f);
				borderColour = beatColour;
			}
			else
			{
				stepColour = subBeatColour.withAlpha(0.3f);
				borderColour = subBeatColour;
			}
		}

		if (i == currentStep && isPlaying && isVisible && currentMeasure == playingMeasure)
		{
			float pulseIntensity = 0.8f + 0.2f * std::sin(juce::Time::getMillisecondCounter() * 0.01f);
			stepColour = ColourPalette::textPrimary.withAlpha(pulseIntensity);
			borderColour = ColourPalette::textPrimary;
		}

		g.setColour(stepColour);
		g.fillRoundedRectangle(stepBounds.toFloat(), 3.0f);
		g.setColour(borderColour);
		g.drawRoundedRectangle(stepBounds.toFloat(), 3.0f, isVisible ? 1.0f : 0.5f);

		if (isVisible)
		{
			g.setColour(ColourPalette::textPrimary.withAlpha(isStrongBeat ? 0.9f : 0.6f));
			g.setFont(juce::FontOptions(9.0f, isStrongBeat ? juce::Font::bold : juce::Font::plain));
			g.drawText(juce::String(i + 1), stepBounds, juce::Justification::centred);
		}
	}

	if (isPlaying)
	{
		auto ledBounds = juce::Rectangle<int>(bounds.getWidth() - 30, 12, 15, 15);
		float pulseIntensity = 0.6f + 0.4f * std::sin(juce::Time::getMillisecondCounter() * 0.008f);
		juce::Colour ledColour = ColourPalette::playActive.withAlpha(pulseIntensity);

		g.setColour(ledColour);
		g.fillEllipse(ledBounds.toFloat());
		g.setColour(ColourPalette::textPrimary.withAlpha(0.8f));
		g.drawEllipse(ledBounds.toFloat(), 1.0f);
	}
}

juce::Rectangle<int> SequencerComponent::getStepBounds(int step)
{
	int totalSteps = getTotalStepsForCurrentSignature();

	float stepsAreaWidthPercent = 0.98f;
	float marginPercent = 0.005f;

	int componentWidth = getWidth();

	int availableWidth = static_cast<int>(componentWidth * stepsAreaWidthPercent);

	int totalMargins = static_cast<int>((totalSteps - 1) * marginPercent * componentWidth);
	int stepWidth = (availableWidth - totalMargins) / totalSteps;
	int marginPixels = static_cast<int>(marginPercent * componentWidth);

	int stepHeight = juce::jmin(stepWidth, 40);

	int totalUsedWidth = totalSteps * stepWidth + (totalSteps - 1) * marginPixels;
	int startX = (componentWidth - totalUsedWidth) / 2;
	int startY = 50;

	int x = startX + step * (stepWidth + marginPixels);
	int y = startY;

	return juce::Rectangle<int>(x, y, stepWidth, stepHeight);
}

void SequencerComponent::mouseDown(const juce::MouseEvent& event)
{
	int totalSteps = getTotalStepsForCurrentSignature();

	for (int i = 0; i < totalSteps; ++i)
	{
		if (getStepBounds(i).contains(event.getPosition()))
		{
			isEditing = true;
			toggleStep(i);
			repaint();
			juce::Timer::callAfterDelay(50, [this]()
				{ isEditing = false; });
			return;
		}
	}
}

int SequencerComponent::getTotalStepsForCurrentSignature() const
{
	int numerator = audioProcessor.getTimeSignatureNumerator();
	int denominator = audioProcessor.getTimeSignatureDenominator();

	int stepsPerBeat;
	if (denominator == 8)
	{
		stepsPerBeat = 2;
	}
	else if (denominator == 4)
	{
		stepsPerBeat = 4;
	}
	else if (denominator == 2)
	{
		stepsPerBeat = 8;
	}
	else
	{
		stepsPerBeat = 4;
	}

	return numerator * stepsPerBeat;
}

void SequencerComponent::toggleStep(int step)
{
	TrackData* track = audioProcessor.getTrack(trackId);
	if (!track) return;

	int safeMeasure = juce::jlimit(0, MAX_MEASURES - 1, currentMeasure);

	if (track->usePages.load()) {
		if (!track->sequencerData.steps[safeMeasure][step]) {
			track->sequencerData.steps[safeMeasure][step] = true;
			stepPages[safeMeasure][step] = 0;
			track->sequencerData.stepPages[safeMeasure][step] = 0;
		}
		else {
			int currentPage = stepPages[safeMeasure][step];
			if (currentPage < 4) {
				stepPages[safeMeasure][step] = currentPage + 1;
				track->sequencerData.stepPages[safeMeasure][step] = currentPage + 1;
			}
			else {
				track->sequencerData.steps[safeMeasure][step] = false;
				stepPages[safeMeasure][step] = 0;
				track->sequencerData.stepPages[safeMeasure][step] = 0;
			}
		}
	}
	else {
		track->sequencerData.steps[safeMeasure][step] = !track->sequencerData.steps[safeMeasure][step];
	}

	track->sequencerData.velocities[safeMeasure][step] = 0.8f;
}

void SequencerComponent::setCurrentStep(int step)
{
	int totalSteps = getTotalStepsForCurrentSignature();
	currentStep = step % totalSteps;
	repaint();
}

void SequencerComponent::resized()
{
	int controlsWidth = 250;

	auto bounds = getLocalBounds();
	bounds.removeFromTop(10);
	bounds.removeFromLeft(13);

	auto topArea = bounds.removeFromTop(30);
	auto controlArea = topArea.removeFromLeft(juce::jmin(controlsWidth, bounds.getWidth() / 2));

	auto pageArea = controlArea.removeFromLeft(120);
	prevMeasureButton.setBounds(pageArea.removeFromLeft(25));
	measureLabel.setBounds(pageArea.removeFromLeft(40));
	nextMeasureButton.setBounds(pageArea.removeFromLeft(25));

	if (topArea.getWidth() > 50)
	{
		currentPlayingMeasureLabel.setBounds(topArea.removeFromLeft(50));
		TrackData* track = audioProcessor.getTrack(trackId);
		if (track && track->usePages.load() && topArea.getWidth() > 120) {
			topArea.removeFromLeft(5);
			pageHelpLabel.setBounds(topArea.removeFromLeft(120));
		}
	}

	if (controlArea.getWidth() > 80)
	{
		controlArea.removeFromLeft(5);
		measureSlider.setBounds(controlArea.removeFromLeft(80));
	}
}

void SequencerComponent::setCurrentMeasure(int measure)
{
	currentMeasure = juce::jlimit(0, numMeasures - 1, measure);
	measureLabel.setText(juce::String(currentMeasure + 1) + "/" + juce::String(numMeasures),
		juce::dontSendNotification);
	repaint();
}

void SequencerComponent::setNumMeasures(int measures)
{
	int oldNumMeasures = numMeasures;
	numMeasures = juce::jlimit(1, MAX_MEASURES, measures);

	if (currentMeasure >= numMeasures)
	{
		setCurrentMeasure(numMeasures - 1);
	}

	TrackData* track = audioProcessor.getTrack(trackId);
	if (track)
	{
		track->sequencerData.numMeasures = numMeasures;

		if (numMeasures < oldNumMeasures)
		{
			int maxSteps = getTotalStepsForCurrentSignature();
			for (int m = numMeasures; m < oldNumMeasures; ++m)
			{
				for (int s = 0; s < maxSteps; ++s)
				{
					track->sequencerData.steps[m][s] = false;
					track->sequencerData.velocities[m][s] = 0.8f;
				}
			}
		}
	}

	measureLabel.setText(juce::String(currentMeasure + 1) + "/" + juce::String(numMeasures),
		juce::dontSendNotification);
	repaint();
}

void SequencerComponent::updateFromTrackData()
{
	if (isEditing)
		return;
	TrackData* track = audioProcessor.getTrack(trackId);
	if (track)
	{
		for (int m = 0; m < 4; ++m) {
			for (int s = 0; s < 16; ++s) {
				stepPages[m][s] = track->sequencerData.stepPages[m][s];
			}
		}
		bool showPageHelp = track->usePages.load();
		pageHelpLabel.setVisible(showPageHelp);

		if (showPageHelp) {
			pageHelpLabel.setText("Click: A>>B>>C>>D>>USER>>Off", juce::dontSendNotification);
		}
		int totalSteps = getTotalStepsForCurrentSignature();
		currentStep = juce::jlimit(0, totalSteps - 1, track->sequencerData.currentStep);
		isPlaying = track->isCurrentlyPlaying;
		numMeasures = track->sequencerData.numMeasures;
		measureSlider.setValue(track->sequencerData.numMeasures);
		measureLabel.setText(juce::String(currentMeasure + 1) + "/" + juce::String(numMeasures),
			juce::dontSendNotification);
		if (isPlaying)
		{
			int playingMeasure = track->sequencerData.currentMeasure + 1;
			currentPlayingMeasureLabel.setText("M " + juce::String(playingMeasure),
				juce::dontSendNotification);
			currentPlayingMeasureLabel.setColour(juce::Label::textColourId, ColourPalette::playActive);
		}
		else
		{
			track->sequencerData.currentStep = 0;
			track->sequencerData.currentMeasure = 0;
			currentPlayingMeasureLabel.setText("M " + juce::String(track->sequencerData.currentMeasure + 1),
				juce::dontSendNotification);
			currentPlayingMeasureLabel.setColour(juce::Label::textColourId, ColourPalette::textSecondary);
		}
		repaint();
	}
}
