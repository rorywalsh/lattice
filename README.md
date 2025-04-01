<div align="center">
<img src="./lattice.svg" alt="Lattice Logo"></img>
<br>
<br>
</div>



Lattice is a simple plugin API that provides a thin wrapper around the CLAP plugin framework. It was initially part of a larger, rapid audio prototyping project that was halted when JUCE announced updates to its EULA for version 8. Rather than let is sit and fester (and give up the chance to use that logo^*), I decided to refactor it using CLAP. Now it forms the glue that holds Cabbage v3 together. Unlike Cabbage, Lattice does not use Csound. In fact, it provides no DSP classes at all (although the example make use of the [Aurora](https://github.com/vlazzarini/aurora) library. Additionally, unlike most plugin frameworks, the UI is entirely provided in a webview. This means you do not have direct access to a traditional 'editor.' Communication between the webview and the plugin processor is handled through various function callbacks that transmit and receive JSON strings.

This wrapper does not provide anywhere near the same level of functionality as the full CLAP framework. However, I'm sharing it in the hope that some people might find it useful. There are no docs, but the included examples should provide with everything you need to get started. 

