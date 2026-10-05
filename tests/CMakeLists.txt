add_executable(shifter_tests
    test_smoke.cpp
    test_ring_buffer.cpp
    test_phase_vocoder.cpp
    test_onset_detector.cpp
)

target_link_libraries(shifter_tests PRIVATE
    shifter_dsp
    Catch2::Catch2WithMain
)

target_compile_features(shifter_tests PRIVATE cxx_std_20)

include(Catch)
catch_discover_tests(shifter_tests)
