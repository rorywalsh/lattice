<div align="center" style="background-color: #2d2d2d; border-radius: 15px; padding: 20px;">
<img src="./lattice.svg" alt="Lattice Logo"></img>
</div>



Lattice is a simple plugin API that provides a thin wrapper around the CLAP plugin framework. It was developed as part of the Cabbage 3 Csound plugin framework. Unlike Cabbage, Lattice does not use Csound. In fact, it provides no DSP classes at all (although the example make use of the [Aurora](https://github.com/vlazzarini/aurora) library. Additionally, unlike most plugin frameworks, the UI is entirely provided in a webview. This means you do not have direct access to the traditional 'editor.' Communication between the webview and the plugin processor is handled through various function callbacks that transmit and receive JSON strings.

This wrapper does not provide nearly the same level of functionality as the full CLAP framework itself. However, I'm sharing it in the hope that some people might find it useful.

#### Usage

The simplest way to get started is to copy and paste, and start hacking a basic example, such as the gain plugin. The GainProcessor is derived from the `lattice::Processor` class, which provides the link between your main processor and the CLAP framework. Any class derived from `lattice::Processor` must implement the following pure virtual functions:

```cpp
// Pure virtual function to process audio data
virtual void process(float** inputs, float** outputs, std::size_t blockSize) = 0;

// Pure virtual function to set a parameter value
virtual void setParameter(int paramId, double value) = 0;

// Pure virtual function to handle messages from the web view
virtual void onMesssgeFromWebView(nlohmann::json j) = 0;

// Prepare to play method - gets called before processing starts
virtual void prepareToPlay(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) = 0;
```

In addition to these functions, there are two function pointers that can be quite useful:

```cpp
    // Function pointer to send parameter updates to the host
    std::function<void(uint32_t, float)> sendParameterUpdateToHost = nullptr;
    
    // Function pointer to send messages to webview
    std::function<void(nlohmann::json)> sendWebViewMessage = nullptr;
```

The web UI can call `window.sendMessageFromUI(jsonMessage)` whenever it needs to send data to the processor. The JSON string can be constructed in any way you wish. In the simple gain example, it consists of a parameter ID and the corresponding value:


```js
// Send the updated value to the host
window.sendMessageFromUI({
    paramIdx: 0,
    value: value
});
```

Messages will be sent to the web UI whenever a parameter is updated or if your processor class calls the `sendWebViewMessage` function. When a parameter update is sent (typically as a result of host automation), a JSON message with the following structure will be sent:

```json
[
    {
        "command": "parameterChange"
        "data":{
                "paramId": number,
                "value": number
        }
    }
]
```

When you send data via the sendWebViewMessage function, it is recommended to use the same structure of command and data fields for consistency.



### Building

Running the CMake generator will download and install all the required frameworks (listed below). To build just run the following commands:

```bash
cmake -GXcode  -S . -B build
cmake --build build
```

The project level `CMakeLists.txt` file has a `generate_plugin_header` that can be used to set various plugin parameters such name, vendor, id, etc. 

### Lattice Dependencies

* [Clap](https://github.com/free-audio/clap.git) - The API this framework is built upon 
* [Clap Wrapper](https://github.com/free-audio/clap-wrapper) - Provides wrapper for various plugin formats
* [Clap Helpers](https://github.com/free-audio/clap-helpers.git) - Various CLAP utility classes
* [Nlohmann JSON](https://github.com/nlohmann/json.git) - Extensive C++ JSON framework  
* [Choc](https://github.com/Tracktion/choc) - A mixed bag of handy C++ classes - provides the webview class for Lattice
* [cpp-httplib](https://github.com/yhirose/cpp-httplib.git) - A HTTP/HTTPS library, used to server the webview sources
