#pragma once
#include <PluginProcessor.h>

/* Since I'm intereacting with APVTs but not opening the GUI some stuff likes to break.
 * This comment on GitHub has some ways to get around that!
 *
 * https://github.com/sudara/pamplejuce/issues/18#issuecomment-1425836807
 */
[[maybe_unused]] static void runWithinPluginEditor (const std::function<void (OpenBendAudioProcessor& plugin)>& testCode)
{
    OpenBendAudioProcessor plugin;
    const auto editor = plugin.createEditorIfNeeded();

    testCode (plugin);

    plugin.editorBeingDeleted (editor);
    delete editor;
}
