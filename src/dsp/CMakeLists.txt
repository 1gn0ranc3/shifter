# DSP code lives here. Kept as a separate static library so tests can link
# against it without pulling in the JUCE plugin client machinery.

add_library(shifter_dsp STATIC
    ring_buffer.h
    mini_fft.h
    mini_fft.cpp
    phase_vocoder.h
    phase_vocoder.cpp
    onset_detector.h
    onset_detector.cpp
)

target_include_directories(shifter_dsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(shifter_dsp PUBLIC cxx_std_20)

target_link_libraries(shifter_dsp PUBLIC
    juce::juce_dsp
)
