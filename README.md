<div align="center">
<img src="./lattice.svg" alt="Lattice Logo"></img>
<br>
<br>
</div>



Lattice is a simple plugin API that provides a thin wrapper around the CLAP plugin framework[^1]. It was initially part of a larger, rapid audio prototyping project that was halted when JUCE announced updates to its EULA for version 8. Rather than let is sit and fester (and give up the chance to use that logo[^2]), I decided to refactor it using CLAP. Now it forms the glue that holds Cabbage v3 together. Unlike Cabbage, Lattice does not use Csound. In fact, it provides no DSP classes at all (although the examples make use of the [Aurora](https://github.com/vlazzarini/aurora) library. Additionally, unlike most plugin frameworks, the UI is handled entirely within a webview. This means you do not have direct access to a traditional 'editor.' Communication between the webview and the plugin processor is handled through various function callbacks that transmit and receive JSON strings.

This wrapper does not provide wrappers to the full range of functions provided by the CLAP framework. However, I'm sharing it in the hope that some people might find it useful. There are no docs, but the included examples should provide users with all the information to get started. 

##### Examples

All the example have a html folder in their source tree. The contents of this folder gets copied to the plugin bundle (if using VST3). If you want to run/build CLAP versions of the plugins, you will need to update the examples and set a custom mount point using the `LatticeProcessor::setMountPoint()` function. 

* Gain: Ubiquitous gain example.

* Simple Synth: A basic synth plugin example with sliders to control the ADSR envelope and waveform.

* MultiChannel: A simple multichannel mixer demonstrating how to set up multichannel plugins.

* Flanger: A basic audio effect plugin.


[^1]: The [ClapPluginCppTemplate](https://github.com/witte/ClapPluginCppTemplate) served as the initial foundation for this project. While the majority of the code has been rewritten to reflect project-specific goals, some elements of the original implementation may still be present.
[^2]: Logo was designed by Sibylle Biggel
