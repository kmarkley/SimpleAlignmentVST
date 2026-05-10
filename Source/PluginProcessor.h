#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────────────────────────────────
namespace SA {

    static constexpr int    NUM_CHANNELS     = 8;

    // Delay parameter limits (milliseconds)
    static constexpr float  SYSTEM_DELAY_MIN =   0.0f;
    static constexpr float  SYSTEM_DELAY_MAX =  30.0f;
    static constexpr float  ALIGN_DELAY_MIN  = -20.0f;
    static constexpr float  ALIGN_DELAY_MAX  = +20.0f;

    // Gain parameter limits (dB)
    static constexpr float  GAIN_MIN         = -12.0f;
    static constexpr float  GAIN_MAX         = +12.0f;

    // Maximum possible effective delay = system max + alignment max (ms)
    // effective_delay[ch] = system_delay + (align_delay[ch] - min(align_delay))
    // worst case: 30ms + (20ms - (-20ms)) = 70ms
    static constexpr float  MAX_DELAY_MS     = 70.0f;

    // Default channel descriptions (index matches channel number)
    static constexpr const char* DEFAULT_NAMES[NUM_CHANNELS] = {
        "Front Left", "Front Right", "LFE", "Center",
        "Surround Left", "Surround Right", "Subwoofer 1", "Subwoofer 2"
    };

} // namespace SA

// ─────────────────────────────────────────────────────────────────────────────
// Parameter ID helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace ParamID {
    static constexpr auto Bypass      = "bypass";
    static constexpr auto SystemDelay = "system_delay";

    // Per-channel IDs are generated at runtime; these are the format strings.
    // e.g. alignDelay(2) → "align_delay_2"
    inline juce::String alignDelay (int ch) { return "align_delay_" + juce::String(ch); }
    inline juce::String chanGain   (int ch) { return "chan_gain_"   + juce::String(ch); }
    // Channel name is stored in a separate ValueTree (not APVTS) because
    // APVTS doesn't natively support string parameters.
    inline juce::String chanName   (int ch) { return "chan_name_"   + juce::String(ch); }
} // namespace ParamID

// ─────────────────────────────────────────────────────────────────────────────
// DelayLine — single-channel circular buffer with linear interpolation.
//
// Write pointer advances each sample; read pointer is offset by the current
// delay in fractional samples.  When effective delay is exactly 0.0, the
// read path is bypassed entirely (direct copy) to guarantee zero latency.
// ─────────────────────────────────────────────────────────────────────────────
class DelayLine
{
public:
    DelayLine();

    /// Allocate the internal buffer.  Call once before audio starts.
    void prepare (int maxDelaySamples);

    /// Clear the buffer (call on stream restart).
    void reset();

    /// Process one block.
    /// @param input        Source samples (read pointer).
    /// @param output       Destination samples (write pointer).
    /// @param numSamples   Block length.
    /// @param delaySamples Fractional delay in samples (0.0 = direct copy).
    void process (const float* input, float* output,
                  int numSamples, float delaySamples) noexcept;

private:
    std::vector<float> mBuffer;
    int                mWritePos { 0 };
    int                mCapacity { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayLine)
};

// ─────────────────────────────────────────────────────────────────────────────
// SimpleAlignmentAudioProcessor
// ─────────────────────────────────────────────────────────────────────────────
class SimpleAlignmentAudioProcessor : public juce::AudioProcessor
{
public:
    SimpleAlignmentAudioProcessor();
    ~SimpleAlignmentAudioProcessor() override;

    // ── AudioProcessor interface ─────────────────────────────────────────────
    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool                        hasEditor()    const override { return true; }

    const juce::String getName() const override { return "SimpleAlignment"; }

    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int  getNumPrograms()                             override { return 1; }
    int  getCurrentProgram()                          override { return 0; }
    void setCurrentProgram (int)                      override {}
    const juce::String getProgramName (int)           override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // ── Parameter tree ───────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;

    // ── Channel names (string parameters, stored alongside APVTS state) ──────
    // Accessed on the message thread only (UI); not used in processBlock.
    juce::String getChannelName (int ch) const;
    void         setChannelName (int ch, const juce::String& name);

    // ── Normalized values (computed from parameters, read by the editor) ─────
    // These are derived quantities — not stored parameters.
    // Updated atomically in processBlock and also on demand from the UI.
    float getNormalizedDelay (int ch) const;  ///< align_delay[ch] - min(align_delay), ms
    float getNormalizedGain  (int ch) const;  ///< chan_gain[ch]   - max(chan_gain),    dB

    /// Recompute and cache normalized values.  Safe to call from any thread.
    void recomputeNormalized();

private:
    // ── Parameter layout factory ─────────────────────────────────────────────
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ── Delay lines (one per channel) ────────────────────────────────────────
    std::array<DelayLine, SA::NUM_CHANNELS> mDelayLines;

    // ── Channel name storage (separate ValueTree, not APVTS) ─────────────────
    juce::ValueTree mNamesTree { "ChannelNames" };

    // ── Runtime sample rate (captured in prepareToPlay) ──────────────────────
    double mSampleRate { 48000.0 };

    // ── Cached normalized values (updated by recomputeNormalized) ────────────
    std::array<std::atomic<float>, SA::NUM_CHANNELS> mNormDelay;
    std::array<std::atomic<float>, SA::NUM_CHANNELS> mNormGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleAlignmentAudioProcessor)
};
