/*
    MIT License

    Copyright (c) 2025 Rory Walsh

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#pragma once

#include <cstddef>
#include <vector>
#include <lattice/clap/ClapPlugin.h>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <list>

// Define the operating system
#if defined (_WIN32) || defined (_WIN64)
#define LATTICE_WINDOWS 1
#elif __APPLE__
#define LATTICE_MACOS 1
#elif defined (LINUX) || defined (__linux__)
#define LATTICE_LINUX 1
#endif


namespace lattice
{

class Processor
{
    // Structure to represent a parameter
    struct Parameter
    {
        std::string name;   // Parameter name
        float min;          // Minimum value
        float max;          // Maximum value
        float value;        // Current value
        float skew;         // Skew factor for scaling
        float increment;    // Step size for parameter change

        // Constructor with default values
        Parameter(const std::string& paramName = "", float paramMin = 0.f, float paramMax = 1.f,
                  float paramValue = 0.f, float paramIncrement = 0.01f, float paramSkew = 1.f)
            : name(paramName), min(paramMin), max(paramMax),
              value(paramValue), skew(paramSkew), increment(paramIncrement)
        {}
    };

public:
    
    struct NoteEvent {
        // Enum for event types
        enum class Type : uint16_t {
            noteOn = 0,    // CLAP_EVENT_NOTE_ON
            noteOff = 1,   // CLAP_EVENT_NOTE_OFF
            noteChoke = 2, // CLAP_EVENT_NOTE_CHOKE
            // Add other event types as needed
        };

        Type type;         // Event type (noteOn, noteOff, noteChoke, etc.)
        int16_t key;       // MIDI note number
        double velocity;   // Normalized velocity (0.0 to 1.0)
        int32_t noteId;    // Unique note ID
        uint32_t sampleOffset; // Sample offset

        // Constructor
        NoteEvent(Type t, int16_t k, double v, int32_t id, uint32_t offset)
            : type(t), key(k), velocity(v), noteId(id), sampleOffset(offset) {}

        // Log the event details
        void log() const {
            std::cout << "{ "
                      << "type: " << static_cast<uint16_t>(type) << ", "
                      << "key: " << key << ", "
                      << "velocity: " << velocity << ", "
                      << "noteId: " << noteId << ", "
                      << "sampleOffset: " << sampleOffset << " }" << std::endl;
        }
    };
    
    // Constructor: Initializes the plugin with a given number of inputs and outputs
    Processor(int numInputs, int numOutputs)
        : numInputs(numInputs), numOutputs(numOutputs)
    {
        parameters.reserve(10);
    }

    // Virtual destructor
    virtual ~Processor(){}

    // Pure virtual function to process audio data
    virtual void process(float** inputs, float** outputs, std::size_t blockSize) = 0;

    // Pure virtual function to set a parameter value
    virtual void setParameter(int paramId, double value) = 0;

    // Pure virtual function to handle messages from the web view
    virtual void onMesssgeFromWebView(nlohmann::json j) = 0;

    // Function to get a parameter value
    double getParameter(int paramId){   return getParameters()[paramId].value;  }

    // Function to get parameter value via the parameter's name
    double getParameter(const std::string& name)
    {
        auto it = parameterMap.find(name);
        if (it != parameterMap.end())
        {
            return it->second->value;
        }

        return 0.0;
    }


    // Prepare to play method - gets called before processing starts
    virtual void prepareToPlay(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) = 0;
    
    void setEditorSize(int w, int h)
    {
        editorWidth = w;
        editorHeight = h;
    }

    // Get the editor width and height
    int getEditorWidth(){   return editorWidth;     }
    int getEditorHeight(){   return editorHeight;   }
    
    // Get the number of audio outputs and inputs
    int getNumOutputs(){    return numOutputs;      }
    int getNumInputs(){     return numInputs;       }

    // Get the list of parameters
    std::vector<Parameter>& getParameters()
    {
        return parameters;
    }

    // Add a new parameter to the list
    void addParameter(lattice::Processor::Parameter parameter)
    {
        std::cout << "Before adding parameter: size = " << parameters.size()
                  << ", capacity = " << parameters.capacity() << std::endl;

        parameters.push_back(parameter);
        parameterMap[parameter.name] = &parameters.back(); // Fast lookup

        std::cout << "After adding parameter: size = " << parameters.size()
                  << ", capacity = " << parameters.capacity() << std::endl;
    }
    
    // Function pointer to send parameter updates to the host
    std::function<void(uint32_t, float)> sendParameterUpdateToHost = nullptr;
    
    // Function pointer to send messages to webview
    std::function<void(nlohmann::json)> sendWebViewMessage = nullptr;

    // This function gets called from the plugin process block and
    // gets filled with note events - there is no need to manually fill it
    void addNoteEvent(NoteEvent noteEvent)
    {
        noteEvents.push_back(noteEvent);
    }
    
    // Note events can be accessed here
    std::deque<NoteEvent>& getNoteEvents()
    {     
        return noteEvents;
    }


private:
    int numInputs = 0;   // Number of audio inputs
    int numOutputs = 0;  // Number of audio outputs
    int editorWidth = 800;
    int editorHeight = 800;
    std::vector<Parameter> parameters; // Vector of parameters
    std::unordered_map<std::string, Parameter*> parameterMap; // Map for fast lookup by name
    std::deque<NoteEvent> noteEvents; // Fifo for noteEvents
};

} // namespace cabs
