// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginEditor.h"

// ─────────────────────────────────────────────────────────────────────────────
// Shared style helpers
// ─────────────────────────────────────────────────────────────────────────────

static void styleTextEditor (juce::TextEditor& ed)
{
    ed.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff2a2a2a));
    ed.setColour (juce::TextEditor::textColourId,       juce::Colours::lightgrey);
    ed.setColour (juce::TextEditor::outlineColourId,    juce::Colour (0xff555555));
    ed.setFont   (juce::Font (13.0f));
    ed.setJustification (juce::Justification::centredRight);
}

static void styleLabel (juce::Label& lbl, bool isHeader = false)
{
    lbl.setColour (juce::Label::textColourId, isHeader
                   ? juce::Colour (0xffaaaaaa)
                   : juce::Colours::lightgrey);
    lbl.setFont   (juce::Font (isHeader ? 11.0f : 12.0f,
                               isHeader ? juce::Font::bold : juce::Font::plain));
    lbl.setJustificationType (juce::Justification::centredRight);
}

// ─────────────────────────────────────────────────────────────────────────────
// ChannelRow
// ─────────────────────────────────────────────────────────────────────────────

ChannelRow::ChannelRow (int channelIndex,
                        SimpleAlignmentAudioProcessor& processor,
                        juce::AudioProcessorValueTreeState& apvts)
    : mChannel  (channelIndex),
      mProcessor (processor)
{
    // ── Name editor ───────────────────────────────────────────────────────────
    mNameEditor.setText (processor.getChannelName (channelIndex), false);
    styleTextEditor (mNameEditor);
    mNameEditor.setJustification (juce::Justification::centredLeft);
    mNameEditor.setTooltip ("Editable channel name — persists across restarts");
    mNameEditor.addListener (this);
    addAndMakeVisible (mNameEditor);

    // ── Delay: hidden slider + visible text editor ────────────────────────────
    mDelaySlider.setRange (SA::ALIGN_DELAY_MIN, SA::ALIGN_DELAY_MAX, 0.0001);
    mDelaySlider.setVisible (false);
    addAndMakeVisible (mDelaySlider);

    mDelayAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamID::alignDelay (channelIndex), mDelaySlider);

    mDelayEditor.setInputFilter (&mDelayFilter, false);
    mDelayEditor.setText (juce::String (mDelaySlider.getValue(), 4), false);
    styleTextEditor (mDelayEditor);
    mDelayEditor.setTooltip ("Raw alignment delay for this channel (−20 to +20 ms)");
    mDelayEditor.addListener (this);
    addAndMakeVisible (mDelayEditor);

    // ── Normalized delay display ──────────────────────────────────────────────
    styleLabel (mNormDelayLabel);
    mNormDelayLabel.setText ("0.0000 ms", juce::dontSendNotification);
    mNormDelayLabel.setTooltip ("Computed delay applied to this channel: alignment delay minus "
                                "the minimum across all channels (always \xe2\x89\xa5 0)");
    addAndMakeVisible (mNormDelayLabel);

    // ── Gain: hidden slider + visible text editor ─────────────────────────────
    mGainSlider.setRange (SA::GAIN_MIN, SA::GAIN_MAX, 0.01);
    mGainSlider.setVisible (false);
    addAndMakeVisible (mGainSlider);

    mGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamID::chanGain (channelIndex), mGainSlider);

    mGainEditor.setInputFilter (&mGainFilter, false);
    mGainEditor.setText (juce::String (mGainSlider.getValue(), 2), false);
    styleTextEditor (mGainEditor);
    mGainEditor.setTooltip ("Raw gain trim for this channel (\xe2\x88\x9212 to +12 dB)");
    mGainEditor.addListener (this);
    addAndMakeVisible (mGainEditor);

    // ── Normalized gain display ───────────────────────────────────────────────
    styleLabel (mNormGainLabel);
    mNormGainLabel.setText ("0.00 dB", juce::dontSendNotification);
    mNormGainLabel.setTooltip ("Computed gain applied to this channel: gain minus the maximum "
                               "across all channels (always \xe2\x89\xa4 0 dB)");
    addAndMakeVisible (mNormGainLabel);

    // ── Mute button ───────────────────────────────────────────────────────────
    mMuteButton.setClickingTogglesState (true);
    mMuteButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff333333));
    mMuteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffbb3333));
    mMuteButton.setColour (juce::TextButton::textColourOffId,  juce::Colours::lightgrey);
    mMuteButton.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    mMuteButton.setTooltip ("Silence this channel");
    addAndMakeVisible (mMuteButton);
    mMuteAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamID::chanMute (channelIndex), mMuteButton);

    // ── Invert button ─────────────────────────────────────────────────────────
    mInvertButton.setClickingTogglesState (true);
    mInvertButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff333333));
    mInvertButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff886600));
    mInvertButton.setColour (juce::TextButton::textColourOffId,  juce::Colours::lightgrey);
    mInvertButton.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    mInvertButton.setTooltip ("Invert polarity of this channel");
    addAndMakeVisible (mInvertButton);
    mInvertAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamID::chanInvert (channelIndex), mInvertButton);
}

ChannelRow::~ChannelRow()
{
    mNameEditor.removeListener (this);
    mDelayEditor.removeListener (this);
    mGainEditor.removeListener (this);
}

void ChannelRow::resized()
{
    const int h = getHeight();
    int x = 0;

    mNameEditor.setBounds     (x, 0, COL_NAME,       h); x += COL_NAME       + COL_GAP;
    mDelayEditor.setBounds    (x, 0, COL_DELAY,      h); x += COL_DELAY      + COL_GAP;
    mNormDelayLabel.setBounds (x, 0, COL_NORM_DELAY, h); x += COL_NORM_DELAY + COL_GAP;
    mGainEditor.setBounds     (x, 0, COL_GAIN,       h); x += COL_GAIN       + COL_GAP;
    mNormGainLabel.setBounds  (x, 0, COL_NORM_GAIN,  h); x += COL_NORM_GAIN  + COL_GAP;
    mMuteButton.setBounds     (x, 0, COL_MUTE,       h); x += COL_MUTE       + COL_GAP;
    mInvertButton.setBounds   (x, 0, COL_INVERT,     h);
}

void ChannelRow::updateNormalizedDisplays()
{
    // Refresh the text editor values from the sliders (in case an automation
    // or another part of the UI changed them)
    mDelayEditor.setText (juce::String (mDelaySlider.getValue(), 4), false);
    mGainEditor.setText  (juce::String (mGainSlider.getValue(),  2), false);

    // Update normalized display labels
    const float nd = mProcessor.getNormalizedDelay (mChannel);
    const float ng = mProcessor.getNormalizedGain  (mChannel);

    // Format with sign and units
    juce::String delayStr = (nd >= 0 ? "+" : "")
                          + juce::String (nd, 4) + " ms";
    juce::String gainStr  = (ng >= 0 ? "+" : "")
                          + juce::String (ng, 2) + " dB";

    mNormDelayLabel.setText (delayStr, juce::dontSendNotification);
    mNormGainLabel.setText  (gainStr,  juce::dontSendNotification);
}

void ChannelRow::setLocked (bool locked)
{
    mNameEditor.setEnabled   (!locked);
    mDelayEditor.setEnabled  (!locked);
    mGainEditor.setEnabled   (!locked);
    mMuteButton.setEnabled   (!locked);
    mInvertButton.setEnabled (!locked);

    const juce::Colour lockedColour  (0xff1a1a1a);
    const juce::Colour normalColour  (0xff2a2a2a);
    auto bg = locked ? lockedColour : normalColour;

    for (auto* ed : { &mNameEditor, &mDelayEditor, &mGainEditor })
        ed->setColour (juce::TextEditor::backgroundColourId, bg);
}

void ChannelRow::commitEditor (juce::TextEditor& editor,
                                juce::Slider&     slider,
                                float             minVal,
                                float             maxVal)
{
    float val = editor.getText().getFloatValue();
    val = juce::jlimit (minVal, maxVal, val);
    slider.setValue (val, juce::sendNotificationAsync);
    editor.setText  (juce::String (val, (&editor == &mDelayEditor) ? 4 : 2), false);
}

void ChannelRow::textEditorReturnKeyPressed (juce::TextEditor& ed)
{
    if      (&ed == &mNameEditor)  mProcessor.setChannelName (mChannel, ed.getText());
    else if (&ed == &mDelayEditor) commitEditor (ed, mDelaySlider, SA::ALIGN_DELAY_MIN, SA::ALIGN_DELAY_MAX);
    else if (&ed == &mGainEditor)  commitEditor (ed, mGainSlider,  SA::GAIN_MIN,        SA::GAIN_MAX);
}

void ChannelRow::textEditorFocusLost (juce::TextEditor& ed)
{
    // Same action as pressing Return — validate and commit on focus loss
    textEditorReturnKeyPressed (ed);
}

// ─────────────────────────────────────────────────────────────────────────────
// SimpleAlignmentAudioProcessorEditor
// ─────────────────────────────────────────────────────────────────────────────

SimpleAlignmentAudioProcessorEditor::SimpleAlignmentAudioProcessorEditor (
        SimpleAlignmentAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (580, 374);

    // ── Bypass toggle ─────────────────────────────────────────────────────────
    bypassToggle.setTooltip ("Hard bypass: passes audio through with no delay or gain processing");
    addAndMakeVisible (bypassToggle);
    bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamID::Bypass, bypassToggle);

    // ── Lock toggle ───────────────────────────────────────────────────────────
    lockToggle.setTooltip ("Lock all controls to prevent accidental edits");
    addAndMakeVisible (lockToggle);
    lockToggle.onClick = [this] { applyLockState(); };

    // ── System delay ──────────────────────────────────────────────────────────
    systemDelayLabel.setText ("System Delay (ms):", juce::dontSendNotification);
    systemDelayLabel.setTooltip ("Global delay offset added to all channels (0–30 ms)");
    styleLabel (systemDelayLabel);
    systemDelayLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (systemDelayLabel);

    systemDelaySlider.setRange (SA::SYSTEM_DELAY_MIN, SA::SYSTEM_DELAY_MAX, 0.0001);
    systemDelaySlider.setVisible (false);
    addAndMakeVisible (systemDelaySlider);

    systemDelayAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        p.apvts, ParamID::SystemDelay, systemDelaySlider);

    systemDelayEditor.setInputFilter (&systemDelayFilter, false);
    systemDelayEditor.setText (juce::String (systemDelaySlider.getValue(), 4), false);
    systemDelayEditor.setTooltip ("Global delay offset added to all channels (0–30 ms)");
    styleTextEditor (systemDelayEditor);
    systemDelayEditor.onReturnKey = [this] {
        float val = juce::jlimit (SA::SYSTEM_DELAY_MIN, SA::SYSTEM_DELAY_MAX,
                                  systemDelayEditor.getText().getFloatValue());
        systemDelaySlider.setValue (val, juce::sendNotificationAsync);
        systemDelayEditor.setText (juce::String (val, 4), false);
    };
    systemDelayEditor.onFocusLost = [this] {
        systemDelayEditor.onReturnKey();
    };
    addAndMakeVisible (systemDelayEditor);

    // ── System attenuation ────────────────────────────────────────────────────
    systemAttenuationLabel.setText ("Attenuation (dB):", juce::dontSendNotification);
    systemAttenuationLabel.setTooltip ("Global attenuation applied to all channels for system "
                                       "protection / max output limiting (-30 to 0 dB)");
    styleLabel (systemAttenuationLabel);
    systemAttenuationLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (systemAttenuationLabel);

    systemAttenuationSlider.setRange (SA::SYSTEM_ATTEN_MIN, SA::SYSTEM_ATTEN_MAX, 0.01);
    systemAttenuationSlider.setVisible (false);
    addAndMakeVisible (systemAttenuationSlider);

    systemAttenuationAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        p.apvts, ParamID::SystemAttenuation, systemAttenuationSlider);

    systemAttenuationEditor.setInputFilter (&systemAttenuationFilter, false);
    systemAttenuationEditor.setText (juce::String (systemAttenuationSlider.getValue(), 2), false);
    systemAttenuationEditor.setTooltip ("Global attenuation applied to all channels for system "
                                        "protection / max output limiting (-30 to 0 dB)");
    styleTextEditor (systemAttenuationEditor);
    systemAttenuationEditor.onReturnKey = [this] {
        float val = juce::jlimit (SA::SYSTEM_ATTEN_MIN, SA::SYSTEM_ATTEN_MAX,
                                  systemAttenuationEditor.getText().getFloatValue());
        systemAttenuationSlider.setValue (val, juce::sendNotificationAsync);
        systemAttenuationEditor.setText (juce::String (val, 2), false);
    };
    systemAttenuationEditor.onFocusLost = [this] {
        systemAttenuationEditor.onReturnKey();
    };
    addAndMakeVisible (systemAttenuationEditor);

    // ── Column headers ────────────────────────────────────────────────────────
    auto makeHeader = [this] (juce::Label& lbl, const juce::String& text) {
        lbl.setText (text, juce::dontSendNotification);
        styleLabel  (lbl, true);
        addAndMakeVisible (lbl);
    };
    makeHeader (hdrName,      "Channel");
    makeHeader (hdrDelay,     "Align (ms)");
    makeHeader (hdrNormDelay, "Norm Delay");
    makeHeader (hdrGain,      "Gain (dB)");
    makeHeader (hdrNormGain,  "Norm Gain");
    makeHeader (hdrMute,      "Mute");
    makeHeader (hdrInvert,    "\xc3\x98");  // Ø

    hdrName.setTooltip      ("Editable channel name — persists across restarts");
    hdrDelay.setTooltip     ("Raw alignment delay input per channel (\xe2\x88\x9220 to +20 ms)");
    hdrNormDelay.setTooltip ("Effective delay applied: alignment minus the minimum across all channels");
    hdrGain.setTooltip      ("Raw gain trim per channel (\xe2\x88\x9212 to +12 dB)");
    hdrNormGain.setTooltip  ("Effective gain applied: trim minus the maximum across all channels");
    hdrMute.setTooltip      ("Silence individual channels");
    hdrInvert.setTooltip    ("Invert polarity (phase flip) of individual channels");

    // ── Channel rows ──────────────────────────────────────────────────────────
    for (int ch = 0; ch < SA::NUM_CHANNELS; ++ch)
    {
        mRows[ch] = std::make_unique<ChannelRow> (ch, p, p.apvts);
        addAndMakeVisible (*mRows[ch]);
    }

    // ── Start refresh timer at ~10 fps (normalized displays don't need fast updates) ──
    startTimerHz (10);
}

SimpleAlignmentAudioProcessorEditor::~SimpleAlignmentAudioProcessorEditor()
{
    stopTimer();
}

void SimpleAlignmentAudioProcessorEditor::resized()
{
    const int margin  = 10;
    const int ctrlH   = 24;
    const int rowH    = ChannelRow::ROW_HEIGHT;
    const int gap     = ChannelRow::COL_GAP;

    int y = margin;

    // ── Top control bar ───────────────────────────────────────────────────────
    bypassToggle.setBounds (margin, y, 80, ctrlH);
    lockToggle.setBounds   (margin + 90, y, 60, ctrlH);
    y += ctrlH + 10;

    // ── System delay / attenuation bar ────────────────────────────────────────
    systemDelayLabel.setBounds  (margin,       y, 130, ctrlH);
    systemDelayEditor.setBounds (margin + 135, y,  70, ctrlH);

    systemAttenuationLabel.setBounds  (margin + 220, y, 140, ctrlH);
    systemAttenuationEditor.setBounds (margin + 365, y,  70, ctrlH);
    y += ctrlH + 10;

    // ── Column headers ────────────────────────────────────────────────────────
    // Match ChannelRow column positions exactly
    int x = margin;
    hdrName.setBounds      (x, y, ChannelRow::COL_NAME,       ctrlH); x += ChannelRow::COL_NAME       + gap;
    hdrDelay.setBounds     (x, y, ChannelRow::COL_DELAY,      ctrlH); x += ChannelRow::COL_DELAY      + gap;
    hdrNormDelay.setBounds (x, y, ChannelRow::COL_NORM_DELAY, ctrlH); x += ChannelRow::COL_NORM_DELAY + gap;
    hdrGain.setBounds      (x, y, ChannelRow::COL_GAIN,       ctrlH); x += ChannelRow::COL_GAIN       + gap;
    hdrNormGain.setBounds  (x, y, ChannelRow::COL_NORM_GAIN,  ctrlH); x += ChannelRow::COL_NORM_GAIN  + gap;
    hdrMute.setBounds      (x, y, ChannelRow::COL_MUTE,       ctrlH); x += ChannelRow::COL_MUTE       + gap;
    hdrInvert.setBounds    (x, y, ChannelRow::COL_INVERT,     ctrlH);
    y += ctrlH + 4;

    // ── Channel rows ──────────────────────────────────────────────────────────
    for (int ch = 0; ch < SA::NUM_CHANNELS; ++ch)
    {
        mRows[ch]->setBounds (margin, y, getWidth() - 2 * margin, rowH);
        y += rowH + 2;
    }

    // Adjust window height to content
    // (called during construction so setSize here would recurse; omit for safety)
}

void SimpleAlignmentAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff222222));

    // Section divider above the channel table
    const int dividerY = mRows[0]->getY() - 3;
    g.setColour (juce::Colour (0xff444444));
    g.drawHorizontalLine (dividerY, 10.0f, static_cast<float> (getWidth() - 10));

    // Plugin title
    g.setFont (juce::Font (15.0f, juce::Font::bold));
    g.setColour (juce::Colours::white);
    g.drawText ("SimpleAlignment", 0, 0, getWidth(), 20,
                juce::Justification::centred, true);
}

void SimpleAlignmentAudioProcessorEditor::timerCallback()
{
    // Refresh system delay / attenuation editor text if sliders were moved externally
    systemDelayEditor.setText (
        juce::String (systemDelaySlider.getValue(), 4), false);
    systemAttenuationEditor.setText (
        juce::String (systemAttenuationSlider.getValue(), 2), false);

    // Refresh normalized displays in each row
    processorRef.recomputeNormalized();
    for (auto& row : mRows)
        row->updateNormalizedDisplays();
}

void SimpleAlignmentAudioProcessorEditor::applyLockState()
{
    const bool locked = lockToggle.getToggleState();

    systemDelayEditor.setEnabled (!locked);
    systemDelayEditor.setColour  (juce::TextEditor::backgroundColourId,
                                   locked ? juce::Colour (0xff1a1a1a)
                                          : juce::Colour (0xff2a2a2a));

    systemAttenuationEditor.setEnabled (!locked);
    systemAttenuationEditor.setColour  (juce::TextEditor::backgroundColourId,
                                        locked ? juce::Colour (0xff1a1a1a)
                                               : juce::Colour (0xff2a2a2a));

    for (auto& row : mRows)
        row->setLocked (locked);
}
