#include "JuceHeader.h"
#include "MixerChannel.h"
#include <string>

MixerChannel::MixerChannel(const juce::String &trackId, DjIaVstProcessor &processor, TrackData *trackData)
	: trackId(trackId), audioProcessor(processor), track(nullptr)
{
	setupUI();
	setTrackData(trackData);
	addEventListeners();
	updateFromTrackData();
	setupMidiLearn();
}

MixerChannel::~MixerChannel()
{
	removeListener("Volume");
	removeListener("Play");
	removeListener("Stop");
	removeListener("Mute");
	removeListener("Solo");
	removeListener("Pitch");
	removeListener("Fine");
	removeListener("Pan");
}

void MixerChannel::setTrackData(TrackData *trackData)
{
	track = trackData;
	if (track)
	{
		juce::WeakReference<MixerChannel> weakThis(this);
		track->onPlayStateChanged = [weakThis](bool isPlaying)
		{
			DBG("🔄 onPlayStateChanged called with: " << (isPlaying ? "true" : "false"));
			if (weakThis != nullptr)
			{
				juce::MessageManager::callAsync([weakThis]()
												{
            if (weakThis != nullptr && !weakThis->isUpdatingButtons) {
                DBG("🎨 Calling updateButtonColors from onPlayStateChanged");
                weakThis->updateButtonColors(); 
            } });
			}
		};
		track->onArmedStateChanged = [weakThis](bool isArmed)
		{
			if (weakThis != nullptr)
			{
				juce::MessageManager::callAsync([weakThis]()
												{
                    if (weakThis != nullptr) {
						weakThis->isBlinking = true;
				 		weakThis->startTimer(300);
                    } });
			}
		};

		track->onArmedToStopStateChanged = [weakThis](bool isArmedToStop)
		{
			if (weakThis != nullptr)
			{
				juce::MessageManager::callAsync([weakThis]()
												{
                    if (weakThis != nullptr) {
                        weakThis->updateButtonColors();
                    } });
			}
		};
	}
}

void MixerChannel::removeListener(juce::String name)
{
	if (!track || track->slotIndex == -1)
		return;
	juce::String paramName = "slot" + juce::String(track->slotIndex + 1) + name;
	auto *param = audioProcessor.getParameterTreeState().getParameter(paramName);
	if (param)
	{
		param->removeListener(this);
	}
}

void MixerChannel::addListener(juce::String name)
{
	DBG("🔍 addListener called for: " << name);
	DBG("🔍 track exists: " << (track ? "YES" : "NO"));
	if (!track || track->slotIndex == -1)
	{
		DBG("❌ Early return - no track or slotIndex = -1");
		return;
	}
	DBG("🔍 slotIndex: " << track->slotIndex);
	juce::String paramName = "slot" + juce::String(track->slotIndex + 1) + name;
	auto *param = audioProcessor.getParameterTreeState().getParameter(paramName);
	if (param)
	{
		param->addListener(this);
	}
}

void MixerChannel::parameterValueChanged(int parameterIndex, float newValue)
{
	if (!track || track->slotIndex == -1)
		return;

	juce::String slotPrefix = "Slot " + juce::String(track->slotIndex + 1);
	auto &allParams = audioProcessor.AudioProcessor::getParameters();

	if (parameterIndex >= 0 && parameterIndex < allParams.size())
	{
		auto *param = allParams[parameterIndex];
		juce::String paramName = param->getName(256);

		if (juce::MessageManager::getInstance()->isThisTheMessageThread())
		{
			juce::Timer::callAfterDelay(50, [this, paramName, slotPrefix, newValue]()
										{ updateUIFromParameter(paramName, slotPrefix, newValue); });
		}
		else
		{
			juce::MessageManager::callAsync([this, paramName, slotPrefix, newValue]()
											{ juce::Timer::callAfterDelay(50, [this, paramName, slotPrefix, newValue]()
																		  { updateUIFromParameter(paramName, slotPrefix, newValue); }); });
		}
	}
}

void MixerChannel::updateUIFromParameter(const juce::String &paramName,
										 const juce::String &slotPrefix,
										 float newValue)
{
	DBG("📥 Raw parameter value: " << paramName << " = " << newValue);

	if (paramName == slotPrefix + " Volume")
	{
		volumeSlider.setValue(newValue, juce::dontSendNotification);
	}
	else if (paramName == slotPrefix + " Pan")
	{
		float denormalizedPan = newValue * 2.0f - 1.0f;
		panKnob.setValue(denormalizedPan, juce::dontSendNotification);
	}
	else if (paramName == slotPrefix + " Pitch")
	{
		float denormalizedPitch = newValue * 24.0f - 12.0f;
		pitchKnob.setValue(denormalizedPitch, juce::dontSendNotification);
	}
	else if (paramName == slotPrefix + " Fine")
	{
		float denormalizedFine = newValue * 100.0f - 50.0f;
		fineKnob.setValue(denormalizedFine, juce::dontSendNotification);
	}
	else if (paramName == slotPrefix + " Mute")
	{
		bool isMuted = newValue > 0.5f;
		muteButton.setToggleState(isMuted, juce::dontSendNotification);
	}
	else if (paramName == slotPrefix + " Solo")
	{
		bool isSolo = newValue > 0.5f;
		soloButton.setToggleState(isSolo, juce::dontSendNotification);
	}
}
void MixerChannel::parameterGestureChanged(int parameterIndex, bool gestureIsStarting)
{
}

void MixerChannel::setSliderParameter(juce::String name, juce::Slider &slider)
{
	if (!track || track->slotIndex == -1)
		return;
	juce::String paramName = "slot" + juce::String(track->slotIndex + 1) + name;
	auto *param = audioProcessor.getParameters().getParameter(paramName);
	param->setValueNotifyingHost(slider.getValue());
}

void MixerChannel::setButtonParameter(juce::String name, juce::Button &button)
{
	if (!track || track->slotIndex == -1)
		return;

	updateButtonColors();
	juce::String paramName = "slot" + juce::String(track->slotIndex + 1) + name;
	auto *param = audioProcessor.getParameters().getParameter(paramName);

	bool state = button.getToggleState();
	param->setValueNotifyingHost(state ? 1.0f : 0.0f);
}

void MixerChannel::addEventListeners()
{
	volumeSlider.onValueChange = [this]()
	{
		setSliderParameter("Volume", volumeSlider);
	};
	pitchKnob.onValueChange = [this]()
	{
		setSliderParameter("Pitch", pitchKnob);
	};
	fineKnob.onValueChange = [this]()
	{
		setSliderParameter("Fine", fineKnob);
	};
	panKnob.onValueChange = [this]()
	{
		setSliderParameter("Pan", panKnob);
	};
	playButton.onClick = [this]()
	{
		if (track)
		{
			if (!track->isPlaying.load())
			{
				bool shouldArm = playButton.getToggleState();
				track->isArmed = shouldArm;
				if (!shouldArm)
				{
					track->isPlaying = false;
				}
			}
			else if (track->isPlaying.load())
			{
				track->isArmed = false;
				track->isArmedToStop = true;
				playButton.setToggleState(false, juce::dontSendNotification);

				isBlinking = true;
				startTimer(300);
			}
			setButtonParameter("Play", playButton);
		}
	};
	stopButton.onClick = [this]()
	{
		if (track)
		{
			if (track->isPlaying.load() && !track->isArmedToStop.load())
			{
				track->isArmed = false;
				track->isArmedToStop = true;
				playButton.setToggleState(false, juce::dontSendNotification);

				isBlinking = true;
				startTimer(300);
			}
			setButtonParameter("Stop", stopButton);
		}
	};
	muteButton.onClick = [this]()
	{
		if (track)
		{
			track->isMuted = muteButton.getToggleState();
		}
		setButtonParameter("Mute", muteButton);
	};
	soloButton.onClick = [this]()
	{
		if (track)
		{
			bool newSoloState = soloButton.getToggleState();
			track->isSolo = newSoloState;
		}
		setButtonParameter("Solo", soloButton);
	};
	addListener("Volume");
	addListener("Play");
	addListener("Stop");
	addListener("Mute");
	addListener("Solo");
	addListener("Pitch");
	addListener("Fine");
	addListener("Pan");
}

void MixerChannel::timerCallback()
{
	if (isBlinking && track && track->isArmedToStop)
	{
		blinkState = !blinkState;
		stopButton.setColour(juce::TextButton::buttonColourId,
							 blinkState ? juce::Colours::red : juce::Colours::darkred);
	}
	else
	{
		stopTimer();
		isBlinking = false;
		updateButtonColors();
	}
}

void MixerChannel::updateVUMeters()
{
	updateVUMeter();
	repaint();
}

void MixerChannel::updateFromTrackData()
{
	if (!track)
		return;

	trackNameLabel.setText(track->trackName, juce::dontSendNotification);
	volumeSlider.setValue(track->volume.load(), juce::dontSendNotification);
	pitchKnob.setValue(track->bpmOffset, juce::dontSendNotification);
	fineKnob.setValue(track->fineOffset, juce::dontSendNotification);
	panKnob.setValue(track->pan.load(), juce::dontSendNotification);
	muteButton.setToggleState(track->isMuted.load(), juce::dontSendNotification);
	soloButton.setToggleState(track->isSolo.load(), juce::dontSendNotification);

	updateButtonColors();
}

void MixerChannel::paint(juce::Graphics &g)
{
	auto bounds = getLocalBounds();

	juce::Colour bgColour = isSelected ? juce::Colour(0xff3a3a3a) : juce::Colour(0xff2a2a2a);
	g.setColour(bgColour);
	g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

	juce::Colour borderColour = isSelected ? juce::Colour(0xff00ff88) : juce::Colour(0xff404040);
	float borderWidth = isSelected ? 2.0f : 1.0f;
	g.drawRoundedRectangle(bounds.toFloat().reduced(1), 8.0f, borderWidth);

	if (isSelected)
	{
		g.setColour(borderColour.withAlpha(0.3f));
		g.drawRoundedRectangle(bounds.toFloat().reduced(1), 10.0f, 1.0f);
	}
	drawVUMeter(g, bounds);
}

void MixerChannel::drawVUMeter(juce::Graphics &g, juce::Rectangle<int> bounds)
{
	auto vuArea = juce::Rectangle<float>(bounds.getWidth() - 10, 110, 6, bounds.getHeight() - 120);

	g.setColour(juce::Colour(0xff0a0a0a));
	g.fillRoundedRectangle(vuArea, 2.0f);

	g.setColour(juce::Colour(0xff666666));
	g.drawRoundedRectangle(vuArea, 2.0f, 0.5f);

	if (!track)
		return;

	float currentLevel = getCurrentAudioLevel();
	float peakLevel = getPeakLevel();

	int numSegments = 20;
	float segmentHeight = (vuArea.getHeight() - 4) / numSegments;

	for (int i = 0; i < numSegments; ++i)
	{
		fillMeters(vuArea, i, segmentHeight, numSegments, currentLevel, g);
	}

	if (peakLevel > 0.0f)
	{
		int peakSegment = (int)(peakLevel * numSegments);
		if (peakSegment < numSegments)
		{
			float peakY = vuArea.getBottom() - 2 - (peakSegment + 1) * segmentHeight;
			juce::Rectangle<float> peakRect(
				vuArea.getX() + 1, peakY, vuArea.getWidth() - 2, 2);

			g.setColour(juce::Colours::white);
			g.fillRect(peakRect);
		}
	}

	if (peakLevel >= 0.95f)
	{
		auto clipRect = juce::Rectangle<float>(vuArea.getX(), vuArea.getY() - 8, vuArea.getWidth(), 4);
		g.setColour(juce::Colours::red);
		g.fillRoundedRectangle(clipRect, 2.0f);
	}
}

void MixerChannel::fillMeters(juce::Rectangle<float> &vuArea, int i, float segmentHeight, int numSegments, float currentLevel, juce::Graphics &g)
{
	float segmentY = vuArea.getBottom() - 2 - (i + 1) * segmentHeight;
	float segmentLevel = (float)i / numSegments;

	juce::Rectangle<float> segmentRect(
		vuArea.getX() + 1, segmentY, vuArea.getWidth() - 2, segmentHeight - 1);

	juce::Colour segmentColour;
	if (segmentLevel < 0.7f)
		segmentColour = juce::Colours::green;
	else if (segmentLevel < 0.9f)
		segmentColour = juce::Colours::orange;
	else
		segmentColour = juce::Colours::red;

	if (currentLevel >= segmentLevel)
	{
		g.setColour(segmentColour);
		g.fillRoundedRectangle(segmentRect, 1.0f);
	}
	else
	{
		g.setColour(segmentColour.withAlpha(0.1f));
		g.fillRoundedRectangle(segmentRect, 1.0f);
	}
}

void MixerChannel::resized()
{
	auto area = getLocalBounds().reduced(4);
	int width = area.getWidth();

	trackNameLabel.setBounds(area.removeFromTop(20));
	area.removeFromTop(5);

	auto transportArea = area.removeFromTop(60);
	auto topRow = transportArea.removeFromTop(28);
	auto bottomRow = transportArea;

	playButton.setBounds(topRow.removeFromLeft(width / 2 - 2).reduced(2));
	stopButton.setBounds(topRow.removeFromLeft(width / 2 - 2).reduced(2));
	muteButton.setBounds(bottomRow.removeFromLeft(width / 2 - 2).reduced(2));
	soloButton.setBounds(bottomRow.removeFromLeft(width / 2 - 2).reduced(2));

	area.removeFromTop(5);

	auto volumeArea = area.removeFromTop(270);
	volumeSlider.setBounds(volumeArea.reduced(width / 4, 0));

	area.removeFromTop(5);

	auto knobsArea = area.removeFromTop(170);

	auto pitchArea = knobsArea.removeFromTop(50);
	pitchLabel.setBounds(pitchArea.removeFromTop(12));
	pitchKnob.setBounds(pitchArea.reduced(2));

	auto fineArea = knobsArea.removeFromTop(50);
	fineLabel.setBounds(fineArea.removeFromTop(12));
	fineKnob.setBounds(fineArea.reduced(2));

	area.removeFromTop(5);
	auto panArea = knobsArea.removeFromTop(50);
	panLabel.setBounds(panArea.removeFromTop(12));
	panKnob.setBounds(panArea.reduced(2));
}

void MixerChannel::updateVUMeter()
{
	if (!track || !track->isPlaying.load())
	{
		currentAudioLevel *= 0.95f;
		if (peakHoldTimer > 0)
		{
			peakHoldTimer--;
			if (peakHoldTimer == 0)
				peakHold *= 0.9f;
		}
		return;
	}

	float instantLevel = calculateInstantLevel();

	levelHistory.push_back(instantLevel);
	if (levelHistory.size() > 5)
		levelHistory.erase(levelHistory.begin());

	float smoothedLevel = 0.0f;
	for (float level : levelHistory)
		smoothedLevel += level;
	smoothedLevel /= levelHistory.size();

	if (smoothedLevel > currentAudioLevel)
	{
		currentAudioLevel = smoothedLevel;
	}
	else
	{
		currentAudioLevel = currentAudioLevel * 0.85f + smoothedLevel * 0.15f;
	}

	if (currentAudioLevel > peakHold)
	{
		peakHold = currentAudioLevel;
		peakHoldTimer = 30;
	}
}

float MixerChannel::calculateInstantLevel()
{
	if (!track || track->numSamples == 0)
		return 0.0f;

	double readPos = track->readPosition.load();
	int sampleIndex = (int)(readPos);

	if (sampleIndex >= 0 && sampleIndex < track->numSamples)
	{
		float level = 0.0f;
		int samples = std::min(32, track->numSamples - sampleIndex);

		for (int i = 0; i < samples; ++i)
		{
			for (int ch = 0; ch < track->audioBuffer.getNumChannels(); ++ch)
			{
				float sample = track->audioBuffer.getSample(ch, sampleIndex + i);
				level += std::abs(sample);
			}
		}

		level /= (samples * track->audioBuffer.getNumChannels());
		level *= track->volume.load();

		return juce::jlimit(0.0f, 1.0f, level * 3.0f);
	}

	return 0.0f;
}

void MixerChannel::setSelected(bool selected)
{
	isSelected = selected;
	repaint();
}

void MixerChannel::setCurrentLevel(float level)
{
	currentAudioLevel = level;
}

void MixerChannel::setupUI()
{
	addAndMakeVisible(trackNameLabel);
	trackNameLabel.setText("Track", juce::dontSendNotification);
	trackNameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
	trackNameLabel.setJustificationType(juce::Justification::centred);
	trackNameLabel.setFont(juce::Font(12.0f, juce::Font::bold));

	addAndMakeVisible(playButton);
	playButton.setButtonText("ARM");
	playButton.setClickingTogglesState(true);

	addAndMakeVisible(stopButton);
	stopButton.setButtonText("STP");
	stopButton.setClickingTogglesState(false);

	addAndMakeVisible(muteButton);
	muteButton.setButtonText("M");
	muteButton.setClickingTogglesState(true);

	addAndMakeVisible(soloButton);
	soloButton.setButtonText("S");
	soloButton.setClickingTogglesState(true);

	addAndMakeVisible(volumeSlider);
	volumeSlider.setRange(0.0, 1.0, 0.01);
	volumeSlider.setValue(0.8);
	volumeSlider.setSliderStyle(juce::Slider::LinearVertical);
	volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
	volumeSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00ff88));
	volumeSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff404040));

	addAndMakeVisible(pitchKnob);
	pitchKnob.setRange(-12.0, 12.0, 0.1);
	pitchKnob.setValue(track ? track->bpmOffset : 0.0, juce::dontSendNotification);
	pitchKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
	pitchKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
	pitchKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00ff88));

	addAndMakeVisible(pitchLabel);
	pitchLabel.setText("PITCH", juce::dontSendNotification);
	pitchLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
	pitchLabel.setJustificationType(juce::Justification::centred);
	pitchLabel.setFont(juce::Font(9.0f));

	addAndMakeVisible(fineKnob);
	fineKnob.setRange(-50.0, 50.0, 1.0);
	fineKnob.setValue(track ? track->fineOffset : 0.0, juce::dontSendNotification);
	fineKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
	fineKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
	fineKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00ff88));

	addAndMakeVisible(fineLabel);
	fineLabel.setText("FINE", juce::dontSendNotification);
	fineLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
	fineLabel.setJustificationType(juce::Justification::centred);
	fineLabel.setFont(juce::Font(9.0f));

	addAndMakeVisible(panKnob);
	panKnob.setRange(-1.0, 1.0, 0.01);
	panKnob.setValue(track ? track->pan.load() : 0.0, juce::dontSendNotification);
	panKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
	panKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
	panKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00ff88));

	addAndMakeVisible(panLabel);
	panLabel.setText("PAN", juce::dontSendNotification);
	panLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
	panLabel.setJustificationType(juce::Justification::centred);
	panLabel.setFont(juce::Font(9.0f));
}

void MixerChannel::updateButtonColors()
{

	if (!track)
	{
		return;
	}
	bool isArmed = track->isArmed.load();
	bool isPlaying = track->isPlaying.load();
	bool isMuted = track->isMuted.load();
	bool isSolo = track->isSolo.load();
	bool isArmedToStop = track->isArmedToStop.load();

	DBG("🔍 Reading isPlaying = " << (isPlaying ? "true" : "false") << " for " << track->trackName);
	DBG("🔍 Reading isArmed = " << (isArmed ? "true" : "false") << " for " << track->trackName);
	DBG("🔍 Reading isMuted = " << (isMuted ? "true" : "false") << " for " << track->trackName);
	DBG("🔍 Reading isSolo = " << (isSolo ? "true" : "false") << " for " << track->trackName);
	DBG("🔍 Reading isArmedToStop = " << (isArmedToStop ? "true" : "false") << " for " << track->trackName);

	playButton.setToggleState(isArmed || isPlaying, juce::dontSendNotification);

	if (isPlaying)
	{
		playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00ff44));
		playButton.setButtonText("PLY");
	}
	else if (isArmed)
	{
		playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffff6600));
		playButton.setButtonText("ARM");
	}
	else
	{
		playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff404040));
		playButton.setButtonText("ARM");
	}

	muteButton.setToggleState(isMuted, juce::dontSendNotification);
	muteButton.setColour(juce::TextButton::buttonOnColourId,
						 juce::Colour(0xffaa0000));
	muteButton.setColour(juce::TextButton::textColourOnId,
						 juce::Colours::white);
	muteButton.setColour(juce::TextButton::buttonColourId,
						 juce::Colour(0xff404040));
	muteButton.setColour(juce::TextButton::textColourOffId,
						 juce::Colours::white);

	soloButton.setToggleState(isSolo, juce::dontSendNotification);
	soloButton.setColour(juce::TextButton::buttonOnColourId,
						 juce::Colour(0xffffff00));
	soloButton.setColour(juce::TextButton::textColourOnId,
						 juce::Colours::black);
	soloButton.setColour(juce::TextButton::buttonColourId,
						 juce::Colour(0xff404040));
	soloButton.setColour(juce::TextButton::textColourOffId,
						 juce::Colours::white);

	stopButton.setColour(juce::TextButton::buttonColourId,
						 (isArmed || isPlaying) ? juce::Colour(0xffaa4400) : juce::Colour(0xff404040));
}

void MixerChannel::learn(juce::String param, std::function<void(float)> uiCallback)
{
	if (audioProcessor.getActiveEditor() && track && track->slotIndex != -1)
	{
		juce::String parameterName = "slot" + juce::String(track->slotIndex + 1) + param;
		juce::String description = "Slot " + juce::String(track->slotIndex + 1) + " " + param;
		audioProcessor.getMidiLearnManager()
			.startLearning(parameterName, &audioProcessor, uiCallback, description);
	}
}

void MixerChannel::setupMidiLearn()
{
	playButton.onMidiLearn = [this]()
	{
		learn("Play");
	};
	muteButton.onMidiLearn = [this]()
	{
		learn("Mute");
	};
	soloButton.onMidiLearn = [this]()
	{
		learn("Solo");
	};
	volumeSlider.onMidiLearn = [this]()
	{
		learn("Volume");
	};
	pitchKnob.onMidiLearn = [this]()
	{
		learn("Pitch");
	};
	fineKnob.onMidiLearn = [this]()
	{
		learn("Fine");
	};
	panKnob.onMidiLearn = [this]()
	{
		learn("Pan");
	};
}