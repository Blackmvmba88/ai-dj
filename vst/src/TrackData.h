struct TrackData
{
	juce::String trackId;
	juce::String trackName;
	int slotIndex = -1;
	std::atomic<bool> isPlaying{ false };
	std::atomic<bool> isArmed{ false };
	juce::String audioFilePath;
	std::atomic<bool> isArmedToStop{ false };
	std::atomic<bool> isCurrentlyPlaying{ false };
	float fineOffset = 0.0f;
	std::atomic<double> cachedPlaybackRatio{ 1.0 };
	juce::AudioSampleBuffer stagingBuffer;
	std::atomic<bool> hasStagingData{ false };
	std::atomic<bool> swapRequested{ false };
	std::function<void(bool)> onPlayStateChanged;
	std::function<void(bool)> onArmedStateChanged;
	std::function<void(bool)> onArmedToStopStateChanged;
	std::atomic<int> stagingNumSamples{ 0 };
	std::atomic<double> stagingSampleRate{ 48000.0 };
	float stagingOriginalBpm = 126.0f;
	double loopStart = 0.0;
	double loopEnd = 4.0;
	float originalBpm = 126.0f;
	int timeStretchMode = 4;
	double timeStretchRatio = 1.0;
	double bpmOffset = 0.0;
	int midiNote = 60;
	juce::AudioSampleBuffer audioBuffer;
	double sampleRate = 48000.0;
	int numSamples = 0;
	std::atomic<bool> isEnabled{ true };
	std::atomic<bool> isSolo{ false };
	std::atomic<bool> isMuted{ false };
	std::atomic<float> volume{ 0.8f };
	std::atomic<float> pan{ 0.0f };
	juce::String prompt;
	juce::String style;
	juce::String stems;
	int customStepCounter = 0;
	double lastPpqPosition = -1.0;
	float bpm = 126.0f;
	std::atomic<double> readPosition{ 0.0 };
	bool showWaveform = false;
	bool showSequencer = false;

	enum class PendingAction {
		None,
		StartOnNextMeasure,
		StopOnNextMeasure
	};

	PendingAction pendingAction = PendingAction::None;

	struct SequencerData {
		bool steps[4][16] = {};
		float velocities[4][16] = {};
		bool isPlaying = false;
		int currentStep = 0;
		int currentMeasure = 0;
		int numMeasures = 1;
		int beatsPerMeasure = 4;
		double stepAccumulator = 0.0;
		double samplesPerStep = 0.0;
	} sequencerData{};

	TrackData() : trackId(juce::Uuid().toString())
	{
		volume = 0.8f;
		pan = 0.0f;
		readPosition = 0.0;
		bpmOffset = 0.0;
		onPlayStateChanged = nullptr;
	}

	~TrackData()
	{
		onPlayStateChanged = nullptr;
		onArmedStateChanged = nullptr;
		onArmedToStopStateChanged = nullptr;
	}

	void reset()
	{
		audioBuffer.setSize(0, 0);
		numSamples = 0;
		readPosition = 0.0;
		isEnabled = true;
		isMuted = false;
		isSolo = false;
		volume = 0.8f;
		pan = 0.0f;
		bpmOffset = 0.0;
	}

	void setPlaying(bool playing)
	{
		bool wasPlaying = isPlaying.load();
		isPlaying = playing;
		if (wasPlaying != playing && onPlayStateChanged && audioBuffer.getNumChannels() > 0 && isPlaying.load())
		{
			juce::MessageManager::callAsync([this, playing]()
				{
					if (onPlayStateChanged) {
						onPlayStateChanged(playing);
					} });
		}
	}

	void setArmed(bool armed)
	{
		bool wasArmed = isArmed.load();
		isArmed = armed;
		if (wasArmed != armed && onArmedStateChanged && audioBuffer.getNumChannels() > 0 && isPlaying.load())
		{
			DBG("🎯 setArmed called on Track " << trackName << " slot " << slotIndex << " -> " << (armed ? "true" : "false"));
			juce::MessageManager::callAsync([this, armed]()
				{
					if (onArmedStateChanged) {
						onArmedStateChanged(armed);
					} });
		}
	}

	void setArmedToStop(bool armedToStop)
	{
		bool wasArmedToStop = isArmedToStop.load();
		isArmedToStop = armedToStop;
		if (wasArmedToStop != armedToStop && onArmedToStopStateChanged && audioBuffer.getNumChannels() > 0 && isPlaying.load())
		{
			DBG("🛑 setArmedToStop called on Track " << trackName << " slot " << slotIndex << " -> " << (armedToStop ? "true" : "false"));
			juce::MessageManager::callAsync([this, armedToStop]()
				{
					if (onArmedToStopStateChanged) {
						onArmedToStopStateChanged(armedToStop);
					} });
		}
	}
};