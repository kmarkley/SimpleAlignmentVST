#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// ─────────────────────────────────────────────────────────────────────────────
// NumericTextFilter — JUCE InputFilter that restricts a TextEditor to valid
// floating-point numbers within a given range.
//
// Allows: digits, one leading minus sign, one decimal point.
// Clamps to [minValue, maxValue] on focus-loss (handled in the editor).
// ─────────────────────────────────────────────────────────────────────────────
class NumericTextFilter : public juce::TextEditor::InputFilter
{
public:
    NumericTextFilter (float minValue, float maxValue)
        : mMin (minValue), mMax (maxValue) {}

    /// Called by JUCE before each keystroke; return empty string to reject.
    juce::String filterNewText (juce::TextEditor& editor,
                                const juce::String& newInput) override
    {
        const juce::String current  = editor.getText();
        const juce::String proposed = current + newInput;

        // Allow partial inputs that could still become valid numbers:
        //   "-"  (start of a negative number)
        //   "."  (start of a decimal)
        //   "-." (start of a negative decimal)
        if (proposed == "-" || proposed == "." || proposed == "-.")
            return newInput;

        // Reject if proposed string isn't parseable as a number
        if (proposed.getDoubleValue() == 0.0 && proposed != "0"
                && !proposed.startsWith("0.") && !proposed.startsWith("-0"))
        {
            // Allow the character only if the resulting string could still
            // be a valid partial number (contains only valid characters)
            for (auto c : newInput)
            {
                if (!juce::CharacterFunctions::isDigit (c)
                    && c != '-' && c != '.')
                    return {};
            }
        }

        return newInput;
    }

    float getMin() const { return mMin; }
    float getMax() const { return mMax; }

private:
    float mMin, mMax;
};

// ─────────────────────────────────────────────────────────────────────────────
// ChannelRow — one row in the alignment table.
//
// Owns all UI elements for a single channel:
//   [Name] [Align Delay] [Norm Delay display] [Gain] [Norm Gain display]
// ─────────────────────────────────────────────────────────────────────────────
class ChannelRow : public juce::Component,
                   private juce::TextEditor::Listener
{
public:
    ChannelRow (int channelIndex,
                SimpleAlignmentAudioProcessor& processor,
                juce::AudioProcessorValueTreeState& apvts);
    ~ChannelRow() override;

    void resized() override;

    /// Refresh the normalized display labels from the processor.
    void updateNormalizedDisplays();

    /// Enable or disable text entry (lock control).
    void setLocked (bool locked);

    // Column widths — kept as statics so the header row can match them.
    static constexpr int COL_NAME       = 130;
    static constexpr int COL_DELAY      = 80;
    static constexpr int COL_NORM_DELAY = 90;
    static constexpr int COL_GAIN       = 70;
    static constexpr int COL_NORM_GAIN  = 80;
    static constexpr int COL_GAP        = 6;
    static constexpr int ROW_HEIGHT     = 26;

private:
    int mChannel;
    SimpleAlignmentAudioProcessor& mProcessor;

    juce::TextEditor mNameEditor;
    juce::TextEditor mDelayEditor;
    juce::Label      mNormDelayLabel;
    juce::TextEditor mGainEditor;
    juce::Label      mNormGainLabel;

    // APVTS attachments for delay and gain
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mDelayAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mGainAttach;

    // Hidden sliders that hold the APVTS parameter values;
    // the visible TextEditors drive these sliders.
    juce::Slider mDelaySlider;
    juce::Slider mGainSlider;

    // Input filters
    NumericTextFilter mDelayFilter { SA::ALIGN_DELAY_MIN, SA::ALIGN_DELAY_MAX };
    NumericTextFilter mGainFilter  { SA::GAIN_MIN,        SA::GAIN_MAX        };

    void textEditorReturnKeyPressed  (juce::TextEditor&) override;
    void textEditorFocusLost         (juce::TextEditor&) override;

    /// Parse and clamp a TextEditor value, then push it to the linked Slider.
    void commitEditor (juce::TextEditor& editor,
                       juce::Slider&     slider,
                       float             minVal,
                       float             maxVal);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelRow)
};

// ─────────────────────────────────────────────────────────────────────────────
// SimpleAlignmentAudioProcessorEditor
// ─────────────────────────────────────────────────────────────────────────────
class SimpleAlignmentAudioProcessorEditor : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit SimpleAlignmentAudioProcessorEditor (SimpleAlignmentAudioProcessor&);
    ~SimpleAlignmentAudioProcessorEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;

private:
    SimpleAlignmentAudioProcessor& processorRef;

    // ── Top controls ─────────────────────────────────────────────────────────
    juce::ToggleButton  bypassToggle   { "Bypass" };
    juce::ToggleButton  lockToggle     { "Lock" };

    juce::Label         systemDelayLabel;
    juce::TextEditor    systemDelayEditor;
    juce::Slider        systemDelaySlider;   ///< hidden; driven by editor

    NumericTextFilter   systemDelayFilter { SA::SYSTEM_DELAY_MIN, SA::SYSTEM_DELAY_MAX };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> systemDelayAttach;

    // ── Column headers ────────────────────────────────────────────────────────
    juce::Label hdrName, hdrDelay, hdrNormDelay, hdrGain, hdrNormGain;

    // ── Channel rows ──────────────────────────────────────────────────────────
    std::array<std::unique_ptr<ChannelRow>, SA::NUM_CHANNELS> mRows;

    // ── Timer ─────────────────────────────────────────────────────────────────
    void timerCallback() override;

    /// Push lock state to all channel rows.
    void applyLockState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleAlignmentAudioProcessorEditor)
};
