add_subdirectory(dsp)

juce_add_plugin(Shifter
    COMPANY_NAME               "Shifter"
    BUNDLE_ID                  "com.shifter.plugin"
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD    ${SHIFTER_COPY_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE   Shft
    PLUGIN_CODE                Shf1
    FORMATS                    VST3
    PRODUCT_NAME               "Shifter"
    VST3_CATEGORIES            "Fx" "Pitch Shift"
)

target_sources(Shifter PRIVATE
    plugin/PluginProcessor.cpp
    plugin/PluginProcessor.h
    plugin/PluginEditor.cpp
    plugin/PluginEditor.h
)

target_compile_definitions(Shifter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_USE_LAME_AUDIO_FORMAT=0
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
)

target_link_libraries(Shifter
    PRIVATE
        shifter_dsp
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
