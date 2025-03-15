<div align="center">
<svg width="180" height="84" viewBox="0 0 30 14" fill="none" xmlns="http://www.w3.org/2000/svg">
    <path d="M3.19273 3.78658C4.03341 3.78658 4.79538 4.11232 5.36528 4.6385L7.30167 2.92842C7.1159 2.63402 7.0057 2.28637 7.0057 1.91366C7.0057 0.85818 7.86527 0 8.9295 0C9.99373 0 10.8533 0.855048 10.8533 1.91366C10.8533 2.26758 10.7557 2.59956 10.5857 2.88457L12.7551 4.69487C13.3313 4.13424 14.1216 3.78658 14.9937 3.78658C15.8313 3.78658 16.5932 4.10918 17.1631 4.63223L19.0869 2.93156C18.898 2.63715 18.7909 2.28637 18.7909 1.91366C18.7909 0.85818 19.6505 0 20.7148 0C21.779 0 22.6386 0.855048 22.6386 1.91366C22.6386 2.26758 22.5409 2.59956 22.3709 2.88143L24.5561 4.69799C25.1354 4.13423 25.9257 3.78658 26.7979 3.78658C28.5674 3.78658 30 5.21163 30 6.97181C30 8.73199 28.5674 10.1571 26.7979 10.1571C25.932 10.1571 25.1448 9.81254 24.5686 9.25505L22.3646 11.1092C22.5378 11.3942 22.6386 11.7293 22.6386 12.0864C22.6386 13.1418 21.779 14 20.7148 14C19.6505 14 18.7909 13.145 18.7909 12.0864C18.7909 11.1374 19.4868 10.3512 20.3999 10.2009V3.79911C20.0661 3.74273 19.7607 3.60494 19.5088 3.40136L17.5882 5.10202C17.9723 5.6282 18.2022 6.27338 18.2022 6.97181C18.2022 8.73199 16.7695 10.1571 15 10.1571C13.2305 10.1571 11.7979 8.73199 11.7979 6.97181C11.7979 6.3047 12.0057 5.68457 12.3583 5.17405L10.1889 3.36377C9.85204 3.64878 9.41754 3.82417 8.93895 3.82417C8.47925 3.82417 8.06049 3.66444 7.72988 3.39822L5.79349 5.10516C6.17762 5.63133 6.40432 6.27338 6.40432 6.97181C6.40432 8.73199 4.9717 10.1571 3.20218 10.1571C1.43266 10.1571 4.47813e-05 8.73199 4.47813e-05 6.97181C-0.00940105 5.21477 1.42322 3.78658 3.19273 3.78658ZM21.9522 10.6331L24.153 8.78211C23.7972 8.26847 23.5863 7.64519 23.5863 6.97495C23.5863 6.31097 23.7909 5.69084 24.1436 5.18032L21.9585 3.36377C21.6971 3.58614 21.376 3.7396 21.0202 3.79911V10.2072C21.376 10.2635 21.694 10.4139 21.9522 10.6331Z" fill="url(#paint0_linear_60_5)"/>
    <defs>
        <linearGradient id="paint0_linear_60_5" x1="29.995" y1="7.00144" x2="-0.0104693" y2="7.00144" gradientUnits="userSpaceOnUse">
            <stop stop-color="#72C0A7"/>
            <stop offset="0.4237" stop-color="#40B5BF"/>
            <stop offset="0.7835" stop-color="#00AECD"/>
            <stop offset="1" stop-color="#00ABD1"/>
        </linearGradient>
    </defs>
</svg>
<br>
  <span style="font-size: 24px; font-weight: bold; font-family: Arial;">Lattice</span>
  <br>
  <br>
</div>


Lattice is a simple plugin API that provides a thin wrapper around the CLAP plugin framework. It was developed as part of the Cabbage 3 Csound plugin framework. Unlike Cabbage, Lattice does not use Csound. In fact, it provides no DSP classes at all. Additionally, unlike most plugin frameworks, the UI is entirely provided in a webview. This means you do not have direct access to the traditional 'editor.' Communication between the webview and the plugin processor is handled through various function callbacks that transmit and receive JSON strings.

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

// Pure virtual function to get a parameter value
virtual double getParameter(int paramId) = 0;

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

### Dependencies

* [Clap](https://github.com/free-audio/clap.git) - The API this framework is built upon 
* [Clap Wrapper](https://github.com/free-audio/clap-wrapper) - Provides wrapper for various plugin formats
* [Clap Helpers](https://github.com/free-audio/clap-helpers.git) - Various CLAP utility classes
* [Nlohmann JSON](https://github.com/nlohmann/json.git) - Extensive C++ JSON framework  
* [Choc](https://github.com/Tracktion/choc) - A mixed bag of handy C++ classes - provides the webview class for Lattice
* [cpp-httplib](https://github.com/yhirose/cpp-httplib.git) - A HTTP/HTTPS library, used to server the webview sources

