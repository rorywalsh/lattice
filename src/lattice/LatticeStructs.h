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

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace lattice {

    // Enum for note event types (shared between input and output)
    enum class NoteEventType : uint16_t
    {
        noteOn = 0,    ///< Note-on event (CLAP_EVENT_NOTE_ON)
        noteOff = 1,   ///< Note-off event (CLAP_EVENT_NOTE_OFF)
        noteChoke = 2,  ///< Note choke event (CLAP_EVENT_NOTE_CHOKE)
        undefined = 9999 ///< Undefined event type (for error handling)
    };

    // Represents a musical note event (e.g., MIDI note on/off).
    struct NoteEvent
    {
        // Keep Type alias for backwards compatibility
        using Type = NoteEventType;

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

    // Represents a note event to be output to the host (MIDI output)
    struct OutputNoteEvent
    {
        NoteEventType type;    ///< Type of note event (noteOn, noteOff, noteChoke)
        int16_t channel;       ///< MIDI channel (0-15)
        int16_t key;           ///< MIDI note number (0-127)
        double velocity;       ///< Normalized velocity (0.0 to 1.0)
        int32_t noteId;        ///< Unique identifier for the note (-1 for wildcard)
        uint32_t sampleOffset; ///< Sample offset within the audio block for sample-accurate timing

        // Default constructor
        OutputNoteEvent()
            : type(NoteEventType::undefined), channel(0), key(0),
              velocity(0.0), noteId(-1), sampleOffset(0) {}

        // Full constructor
        OutputNoteEvent(NoteEventType t, int16_t ch, int16_t k, double v, int32_t id, uint32_t offset)
            : type(t), channel(ch), key(k), velocity(v), noteId(id), sampleOffset(offset) {}

        // Convenience constructor for simple note on/off
        OutputNoteEvent(NoteEventType t, int16_t k, double v, uint32_t offset)
            : type(t), channel(0), key(k), velocity(v), noteId(-1), sampleOffset(offset) {}

        // Prints event details for debugging
        void log() const
        {
            lattice::logInfo << "OutputNoteEvent { type: " << static_cast<uint16_t>(type)
                << ", channel: " << channel
                << ", key: " << key
                << ", velocity: " << velocity
                << ", noteId: " << noteId
                << ", sampleOffset: " << sampleOffset << " }";
        }
    };

    // Enum for identifying raw MIDI message types (for optional parsing/filtering)
    enum class MidiMessageType : uint8_t
    {
        NoteOff         = 0x80,
        NoteOn          = 0x90,
        PolyAftertouch  = 0xA0,
        ControlChange   = 0xB0,
        ProgramChange   = 0xC0,
        ChannelPressure = 0xD0,
        PitchBend       = 0xE0,
        SystemMessage   = 0xF0,
        Unknown         = 0x00
    };

    // Represents raw MIDI data for output to the host
    struct RawMidiEvent
    {
        uint8_t data[3];       ///< Raw MIDI bytes (status, data1, data2)
        uint8_t size;          ///< Number of valid bytes (1-3)
        uint32_t sampleOffset; ///< Sample offset within the audio block for sample-accurate timing

        // Default constructor
        RawMidiEvent()
            : data{0, 0, 0}, size(0), sampleOffset(0) {}

        // Constructor from raw bytes
        RawMidiEvent(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t sz, uint32_t offset)
            : data{b0, b1, b2}, size(sz), sampleOffset(offset) {}

        // Constructor from buffer (e.g., from Csound's WriteMidiData)
        RawMidiEvent(const unsigned char* buffer, int numBytes, uint32_t offset)
            : data{0, 0, 0}, size(static_cast<uint8_t>(numBytes > 3 ? 3 : numBytes)), sampleOffset(offset)
        {
            for (int i = 0; i < size; ++i)
                data[i] = buffer[i];
        }

        // Helper to get message type from status byte
        static MidiMessageType getMessageType(uint8_t statusByte)
        {
            if (statusByte >= 0xF0) return MidiMessageType::SystemMessage;
            uint8_t type = statusByte & 0xF0;
            switch (type) {
                case 0x80: return MidiMessageType::NoteOff;
                case 0x90: return MidiMessageType::NoteOn;
                case 0xA0: return MidiMessageType::PolyAftertouch;
                case 0xB0: return MidiMessageType::ControlChange;
                case 0xC0: return MidiMessageType::ProgramChange;
                case 0xD0: return MidiMessageType::ChannelPressure;
                case 0xE0: return MidiMessageType::PitchBend;
                default:   return MidiMessageType::Unknown;
            }
        }

        // Get message type for this event
        MidiMessageType getType() const { return getMessageType(data[0]); }

        // Get MIDI channel (0-15) from status byte
        uint8_t getChannel() const { return data[0] & 0x0F; }

        // Check if this is a specific message type
        bool isNoteOn() const { return getType() == MidiMessageType::NoteOn && data[2] > 0; }
        bool isNoteOff() const { return getType() == MidiMessageType::NoteOff ||
                                        (getType() == MidiMessageType::NoteOn && data[2] == 0); }
        bool isCC() const { return getType() == MidiMessageType::ControlChange; }
        bool isProgramChange() const { return getType() == MidiMessageType::ProgramChange; }
        bool isPitchBend() const { return getType() == MidiMessageType::PitchBend; }

        // Prints event details for debugging
        void log() const
        {
            lattice::logInfo << "RawMidiEvent { data: ["
                << static_cast<int>(data[0]) << ", "
                << static_cast<int>(data[1]) << ", "
                << static_cast<int>(data[2]) << "], size: "
                << static_cast<int>(size) << ", type: "
                << static_cast<int>(static_cast<uint8_t>(getType()))
                << ", channel: " << static_cast<int>(getChannel())
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

    // Represents host transport/playhead information (tempo, position, time signature)
    struct TransportInfo {
        double songPosBeats = 0.0;       ///< Current position in beats
        double songPosSeconds = 0.0;     ///< Current position in seconds
        double tempo = 120.0;             ///< Current tempo in BPM
        double tempoInc = 0.0;            ///< Tempo increment/change
        double loopStartBeats = 0.0;     ///< Loop start position in beats
        double loopEndBeats = 0.0;       ///< Loop end position in beats
        double loopStartSeconds = 0.0;   ///< Loop start position in seconds
        double loopEndSeconds = 0.0;     ///< Loop end position in seconds
        int32_t timeSigNum = 4;          ///< Time signature numerator
        int32_t timeSigDenom = 4;        ///< Time signature denominator
        double barStart = 0.0;            ///< Bar start position in beats
        int64_t barNumber = 0;           ///< Current bar number
        bool isPlaying = false;          ///< Is transport playing
        bool isRecording = false;        ///< Is transport recording
        bool isLoopActive = false;       ///< Is loop active
    };

} // namespace lattice
