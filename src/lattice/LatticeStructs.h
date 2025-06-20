#pragma once

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

#include <vector>
#include <iostream>
#include <string>
#include "LatticeUtils.h"

namespace lattice {

    // Represents a musical note event (e.g., MIDI note on/off).
    struct NoteEvent
    {
        enum class Type : uint16_t
        {
            noteOn = 0,    ///< Note-on event (CLAP_EVENT_NOTE_ON)
            noteOff = 1,   ///< Note-off event (CLAP_EVENT_NOTE_OFF)
            noteChoke = 2,  ///< Note choke event (CLAP_EVENT_NOTE_CHOKE)
            undefined = 9999 ///< Undefined event type (for error handling)
        };

        Type type;             ///< Type of note event
        int16_t key;           ///< MIDI note number (0-127)
        double velocity;       ///< Normalized velocity (0.0 to 1.0)
        int32_t noteId;        ///< Unique identifier for the note
        uint32_t sampleOffset; ///< Offset in the audio buffer (for timing accuracy)

        // Constructor
        NoteEvent(Type t, int16_t k, double v, int32_t id, uint32_t offset)
            : type(t), key(k), velocity(v), noteId(id), sampleOffset(offset) {}

        // Prints event details for debugging
        void log() const
        {
            lattice::logInfo << "{ type: " << static_cast<uint16_t>(type)
                << ", key: " << key
                << ", velocity: " << velocity
                << ", noteId: " << noteId
                << ", sampleOffset: " << sampleOffset << " }";
        }
    };

    // Enum representing different channel layouts for an audio bus
    enum class ChannelLayout : int
    {
        Mono,   ///< Single-channel (1 channel)
        Stereo  ///< Two-channel (Left/Right)
    };

    // Represents an audio bus (group of channels).
    struct AudioBus
    {
        std::string busName;       ///< Name of the bus
        int numChannels;        ///< Number of channels in the bus
        ChannelLayout layout;   ///< Layout of the channels

        // Constructor
        AudioBus(const std::string& name, int channels, ChannelLayout busLayout)
            : busName(name), numChannels(channels), layout(busLayout) {}
    };


    // Manages input and output audio bus configurations.
    struct ChannelConfig {
        std::vector<AudioBus> inputBuses;  ///< List of input buses
        std::vector<AudioBus> outputBuses; ///< List of output buses

        // Adds an input bus
        void addInputBus(const std::string &name, int numChannels, ChannelLayout layout)
        {
            inputBuses.emplace_back(name + std::to_string(inputBuses.size()), numChannels, layout);
        }

        // Adds an output bus
        void addOutputBus(const std::string &name, int numChannels, ChannelLayout layout)
        {
            outputBuses.emplace_back(name + std::to_string(outputBuses.size()), numChannels, layout);
        }

        size_t getNumInputBuses() const {
            return inputBuses.size();
        }

        size_t getNumOutputBuses() const {
            return outputBuses.size();
        }

        int getNumInputChannels(int busIndex)
        {
            return inputBuses[busIndex].numChannels;
        }

        int getNumOutputChannels(int busIndex)
        {
            return outputBuses[busIndex].numChannels;
        }

        const std::string getInputBusName(int busIndex)
        {
            return inputBuses[busIndex].busName;
        }

        const std::string getOutputBusName(int busIndex)
        {
            return outputBuses[busIndex].busName;
        }

        // Returns the total number of input channels across all input buses
        int getTotalNumInputChannels() const
        {
            int total = 0;
            for (const auto& bus : inputBuses)
            {
                total += bus.numChannels;
            }
            return total;
        }

        // Returns the total number of output channels across all output buses
        int getTotalNumOutputChannels() const
        {
            int total = 0;
            for (const auto& bus : outputBuses)
            {
                total += bus.numChannels;
            }
            return total;
        }
    };


    // Represents an adjustable parameter for an audio plugin.
    struct Parameter {
        std::string name;   ///< Parameter name
        float min;          ///< Minimum value
        float max;          ///< Maximum value
        float value;        ///< Current value
        float skew;         ///< Skew factor for scaling
        float increment;    ///< Step size for parameter change

        // Constructor with default values
        Parameter(const std::string& paramName = "", float paramMin = 0.f, float paramMax = 1.f,
            float paramValue = 0.f, float paramIncrement = 0.01f, float paramSkew = 1.f)
            : name(paramName), min(paramMin), max(paramMax),
            value(paramValue), skew(paramSkew), increment(paramIncrement) {}
        
            // Converts a normalised value [0.0, 1.0] to the parameter's actual range
            float fromNormalised(float normalized) const {
                if (skew == 1.0f) {
                    return min + (max - min) * normalized;
                } else {
                    return min + (max - min) * std::pow(normalized, skew);
                }
            }

            // Converts a full-range value to its normalized [0.0, 1.0] equivalent
            float toNormalised(float actualValue) const {
                float norm = (actualValue - min) / (max - min);
                if (skew == 1.0f || norm <= 0.0f) {
                    return norm;
                } else {
                    return std::pow(norm, 1.0f / skew);
                }
            }
    };

    enum class ParamChangeType {
        GestureBegin,
        Value,
        GestureEnd,
        Complete
    };

    struct ParameterChange {
        int paramId;
        double value = 0.0; //this should always be normalised!!
        lattice::ParamChangeType type;
        uint32_t time = 0;
    };

} // namespace lattice
