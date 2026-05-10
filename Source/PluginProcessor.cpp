#include "PluginProcessor.h"
#include "PluginEditor.h"

// ─────────────────────────────────────────────────────────────────────────────
// DelayLine
// ─────────────────────────────────────────────────────────────────────────────

DelayLine::DelayLine() {}

void DelayLine::prepare (int maxDelaySamples)
{
    mCapacity = maxDelaySamples;
    mBuffer.assign (mCapacity, 0.0f);
    mWritePos = 0;
}

void DelayLine::reset()
{
    std::fill (mBuffer.begin(), mBuffer.end(), 0.0f);
    mWritePos = 0;
}

void DelayLine::process (const float* input, float* output,
                         int numSamples, float delaySamples) noexcept
{
    // ── Zero-delay fast path ──────────────────────────────────────────────────
    // When effective delay is exactly zero, skip the circular buffer entirely.
    // This guarantees no latency is introduced on undelayed channels.
    if (delaySamples <= 0.0f)
    {
        if (input != output)
            std::copy (input, input + numSamples, output);
        return;
    }

    // ── Fractional delay via linear interpolation ─────────────────────────────
    // Separate the delay into an integer part (buffer offset) and a fractional
    // part (blend weight between adjacent samples).
    //
    //   output[n] = buffer[writePos - intDelay - 1] * frac
    //             + buffer[writePos - intDelay    ] * (1 - frac)
    //
    // where frac is the fractional part of delaySamples.

    const int   intDelay  = static_cast<int> (delaySamples);
    const float frac      = delaySamples - static_cast<float> (intDelay);
    const float oneMinFrac = 1.0f - frac;

    for (int i = 0; i < numSamples; ++i)
    {
        // Write current input sample into the circular buffer
        mBuffer[mWritePos] = input[i];

        // Read positions (wrapping)
        int readPos0 = mWritePos - intDelay;
        if (readPos0 < 0) readPos0 += mCapacity;

        int readPos1 = readPos0 - 1;
        if (readPos1 < 0) readPos1 += mCapacity;

        // Linear interpolation: pos1 is one sample older than pos0
        output[i] = mBuffer[readPos0] * oneMinFrac
                  + mBuffer[readPos1] * frac;

        // Advance write pointer
        if (++mWritePos >= mCapacity)
            mWritePos = 0;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Parameter layout
// ─────────────────────────────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout
SimpleAlignmentAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Bypass toggle
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParamID::Bypass, "Bypass", false));

    // System delay: 0.0 – 30.0 ms, default 0.0
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        ParamID::SystemDelay,
        "System Delay",
        juce::NormalisableRange<float> (SA::SYSTEM_DELAY_MIN, SA::SYSTEM_DELAY_MAX, 0.0001f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    // Per-channel alignment delay and gain
    for (int ch = 0; ch < SA::NUM_CHANNELS; ++ch)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            ParamID::alignDelay (ch),
            "Align Delay " + juce::String (ch),
            juce::NormalisableRange<float> (SA::ALIGN_DELAY_MIN, SA::ALIGN_DELAY_MAX, 0.0001f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            ParamID::chanGain (ch),
            "Channel Gain " + juce::String (ch),
            juce::NormalisableRange<float> (SA::GAIN_MIN, SA::GAIN_MAX, 0.01f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));
    }

    return layout;
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

SimpleAlignmentAudioProcessor::SimpleAlignmentAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::discreteChannels (SA::NUM_CHANNELS), true)
          .withOutput ("Output", juce::AudioChannelSet::discreteChannels (SA::NUM_CHANNELS), true)),
      apvts (*this, nullptr, "SimpleAlignmentState", createParameterLayout())
{
    // Initialise atomic arrays
    for (int ch = 0; ch < SA::NUM_CHANNELS; ++ch)
    {
        mNormDelay[ch].store (0.0f);
        mNormGain[ch].store  (0.0f);
    }

    // Seed channel names tree with defaults
    for (int ch = 0; ch < SA::NUM_CHANNELS; ++ch)
        mNamesTree.setProperty (ParamID::chanName (ch),
                                SA::DEFAULT_NAMES[ch], nullptr);
}

SimpleAlignmentAudioProcessor::~SimpleAlignmentAudioProcessor() {}

// ─────────────────────────────────────────────────────────────────────────────
// Bus layout
// ─────────────────────────────────────────────────────────────────────────────

bool SimpleAlignmentAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return (layouts.getMainInputChannelSet()  ==
                juce::AudioChannelSet::discreteChannels (SA::NUM_CHANNELS) &&
            layouts.getMainOutputChannelSet() ==
                juce::AudioChannelSet::discreteChannels (SA::NUM_CHANNELS));
}

// ─────────────────────────────────────────────────────────────────────────────
// Prepare / Release
// ─────────────────────────────────────────────────────────────────────────────

void SimpleAlignmentAudioProcessor::prepareToPlay (double sampleRate,
                                                    int   /*samplesPerBlock*/)
{
    mSampleRate = sampleRate;
    const int maxDelaySamples = static_cast<int> (SA::MAX_DELAY_MS * 0.001f * sampleRate) + 2;
    for (auto& dl : mDelayLines)
        dl.prepare (maxDelaySamples);

    recomputeNormalized();
}

void SimpleAlignmentAudioProcessor::releaseResources()
{
    for (auto& dl : mDelayLines)
        dl.reset();
}

// ─────────────────────────────────────────────────────────────────────────────
// Tail length — report maximum possible delay so the host knows not to cut off
// the plugin's output early when the transport stops.
// ─────────────────────────────────────────────────────────────────────────────

double SimpleAlignmentAudioProcessor::getTailLengthSeconds() const
{
    return SA::MAX_DELAY_MS * 0.001;
}

// ─────────────────────────────────────────────────────────────────────────────
// Normalized value computation
// ─────────────────────────────────────────────────────────────────────────────

void SimpleAlignmentAudioProcessor::recomputeNormalized()
{
    // Read all raw parameter values
    float delays[SA::NUM_CHANNELS], gains[SA::NUM_CHANNELS];
    for (int ch = 0; ch < SA::NUM_CHANNELS; ++ch)
    {
        delays[ch] = apvts.getRawParameterValue (ParamID::alignDelay (ch))->load();
        gains[ch]  = apvts.getRawParameterValue (ParamID::chanGain   (ch))->load();
    }

    // Find the minimum delay and maximum gain across all channels
    float minDelay = *std::min_element (delays, delays + SA::NUM_CHANNELS);
    float maxGain  = *std::max_element (gains,  gains  + SA::NUM_CHANNELS);

    // Store normalized values atomically
    for (int ch = 0; ch < SA::NUM_CHANNELS; ++ch)
    {
        mNormDelay[ch].store (delays[ch] - minDelay, std::memory_order_relaxed);
        mNormGain[ch].store  (gains[ch]  - maxGain,  std::memory_order_relaxed);
    }
}

float SimpleAlignmentAudioProcessor::getNormalizedDelay (int ch) const
{
    return mNormDelay[ch].load (std::memory_order_relaxed);
}

float SimpleAlignmentAudioProcessor::getNormalizedGain (int ch) const
{
    return mNormGain[ch].load (std::memory_order_relaxed);
}

// ─────────────────────────────────────────────────────────────────────────────
// Channel name accessors (message thread only)
// ─────────────────────────────────────────────────────────────────────────────

juce::String SimpleAlignmentAudioProcessor::getChannelName (int ch) const
{
    return mNamesTree.getProperty (ParamID::chanName (ch),
                                   SA::DEFAULT_NAMES[ch]).toString();
}

void SimpleAlignmentAudioProcessor::setChannelName (int ch, const juce::String& name)
{
    mNamesTree.setProperty (ParamID::chanName (ch), name, nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// processBlock — real-time audio engine
// ─────────────────────────────────────────────────────────────────────────────

void SimpleAlignmentAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                   juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = std::min (buffer.getNumChannels(), SA::NUM_CHANNELS);

    // ── Bypass: direct passthrough, no delay or gain ──────────────────────────
    const bool bypass = apvts.getRawParameterValue (ParamID::Bypass)->load() > 0.5f;
    if (bypass)
        return;  // in-place buffer requires no copy for passthrough

    // ── Read parameters ───────────────────────────────────────────────────────
    const float systemDelayMs = apvts.getRawParameterValue (ParamID::SystemDelay)->load();

    // Recompute normalized values so they're current for this block
    recomputeNormalized();

    // ── Process each channel ──────────────────────────────────────────────────
    for (int ch = 0; ch < numChannels; ++ch)
    {
        // Total effective delay in milliseconds:
        //   system_delay + (align_delay[ch] - min(align_delay))
        // The normalized delay is already (align_delay[ch] - min), so:
        const float effectiveDelayMs = systemDelayMs
                                     + mNormDelay[ch].load (std::memory_order_relaxed);

        // Convert ms → fractional samples
        const float delaySamples = effectiveDelayMs * static_cast<float> (mSampleRate)
                                   / 1000.0f;

        // Process delay (writes into the same channel in-place)
        mDelayLines[ch].process (buffer.getReadPointer  (ch),
                                  buffer.getWritePointer (ch),
                                  numSamples,
                                  delaySamples);

        // Apply normalized gain (dB → linear)
        const float normGainDB     = mNormGain[ch].load (std::memory_order_relaxed);
        const float normGainLinear = juce::Decibels::decibelsToGain (normGainDB);
        if (normGainLinear != 1.0f)
            buffer.applyGain (ch, 0, numSamples, normGainLinear);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// State persistence
// ─────────────────────────────────────────────────────────────────────────────

void SimpleAlignmentAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Combine APVTS state and channel names into one XML tree
    auto state = apvts.copyState();
    state.appendChild (mNamesTree.createCopy(), nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SimpleAlignmentAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (!xml) return;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (!tree.isValid()) return;

    // Restore APVTS parameters
    if (tree.hasType (apvts.state.getType()))
        apvts.replaceState (tree);

    // Restore channel names from the embedded child tree
    auto namesChild = tree.getChildWithName ("ChannelNames");
    if (namesChild.isValid())
        mNamesTree = namesChild.createCopy();
}

// ─────────────────────────────────────────────────────────────────────────────
// Plugin entry point
// ─────────────────────────────────────────────────────────────────────────────

juce::AudioProcessorEditor* SimpleAlignmentAudioProcessor::createEditor()
{
    return new SimpleAlignmentAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleAlignmentAudioProcessor();
}
