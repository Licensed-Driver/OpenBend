// All test files are included in the executable via the Glob in CMakeLists.txt

#include "juce_gui_basics/juce_gui_basics.h"
#include <catch2/catch_session.hpp>

int main (int argc, char* argv[])
{
    // Just lets us test GUI and Graphic shit without leaking memory
    juce::ScopedJuceInitialiser_GUI gui;

    const int result = Catch::Session().run (argc, argv);

    return result;
}
#include <catch2/catch_test_macros.hpp>
