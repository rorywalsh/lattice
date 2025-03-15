/*
    MIT License

    Copyright (c) 2024 Rory Walsh

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

namespace lattice
{


class Processor
{
    // Structure to represent a parameter
    struct Parameter
    {
        const char* name;   // Parameter name
        float min;          // Minimum value
        float max;          // Maximum value
        float value;        // Current value
        float skew;         // Skew factor for scaling
        float increment;    // Step size for parameter change

        // Constructor with default values
        Parameter(const char* paramName = "", float paramMin = 0.f, float paramMax = 1.f,
                  float paramValue = 0.f, float paramIncrement = 0.01f, float paramSkew = 1.f)
            : name(paramName), min(paramMin), max(paramMax),
              value(paramValue), skew(paramSkew), increment(paramIncrement)
        {
        }
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
    }

    // Virtual destructor
    virtual ~Processor()
    {
    }

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
    
    // Get the number of audio outputs
    int getNumOutputs()
    {
        return numOutputs;
    }

    // Get the number of audio inputs
    int getNumInputs()
    {
        return numInputs;
    }

    // Get the list of parameters
    std::vector<Parameter>& getParameters()
    {
        return parameters;
    }

    // Add a new parameter to the list
    void addParameter(lattice::Processor::Parameter parameter)
    {
        parameters.push_back(parameter);
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
    std::vector<Parameter> parameters; // Vector of parameters
    std::deque<NoteEvent> noteEvents; // Fifo for noteEvents
};

} // namespace cabs
