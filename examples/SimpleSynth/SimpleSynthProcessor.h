// CawProcessor.h
#pragma once

#include "lattice/LatticeProcessor.h"
#include "BlOsc.h"
#include "Env.h"
#include "OnePole.h"

//===========================================================


class SimpleSynthProcessor : public lattice::Processor 
{
    class Synth {
 
    float absSum(const std::vector<float>& data)
    {
        return std::accumulate(data.begin(), data.end(), 0.0f,
            [](float sum, float val) { return sum + std::abs(val); });
    }
    
    public:
        Synth(int noteNumber = 60, float rt = 0.1, float sr = 44100);
        void setWaveform(int waveForm);
        void setBlockSize(std::size_t blockSize);
        void setSampleRate(int sr);
        
        
        const std::vector<float>& operator()(float a, float f, bool gate) {
            auto& output = env(osc(a, f), gate);
            
            if(absSum(output) > 0)
                int test = 0;
            
            return output;
        }
        
        void setAttack(float value)     {    att = value;           }
        void setDecay(float value)      {    dec = value;           }
        void setSustain(float value)    {    sus = value;           }
        void setRelease(float value)    {    env.release(value);   rel = value; }
        float getAttack()     {    return att;           }
        float getDecay()      {    return dec;           }
        float getSustain()    {    return sus;           }
        float getRelease()    {    return rel;           }
        
        int getNoteNumber() { return midiNoteNumber; }
        void setNoteNumber(int value) { midiNoteNumber = value; }
        void setIsPlaying(bool value) { noteType = value; }
        bool isPlaying() { return noteType; }
        
    private:
        
        float att, dec, sus, rel;
        float currentAmp = 0;
        int midiNoteNumber = 0;
        bool noteType = false;
        Aurora::TableSet<float> sawWave;
        Aurora::TableSet<float> squareWave;
        Aurora::TableSet<float> triangleWave;
        Aurora::BlOsc<float> osc;
        Aurora::Env<float> env;
        Aurora::OnePole<float> lp;
    };

    double getMidiNoteInHertz(const int noteNumber, const double aTuning = 440)
    {
        return aTuning * std::pow(2.0, static_cast<double>(noteNumber - 69) / 12.0);
    }

public:
    // Constructor that initializes the CLAP plugin
    SimpleSynthProcessor();
    
    // Destructor to clean up resources
    ~SimpleSynthProcessor(){};

    // Process method to handle audio processing
    void process(double** inputs, double** outputs, std::size_t blockSize) override;

    // Set a parameter value
    void setParameter(int paramId, double value) override;
    
    // Called whenever the webview sends a message
    void onMessageFromWebView(const nlohmann::json &j) override;
    
    // Called at least once before the processing starts
    void prepareToPlay(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) override;
    
    

private:
    void manageVoices(lattice::NoteEvent noteEvent);
    SimpleSynthProcessor::Synth synthVoices[16];
    SimpleSynthProcessor::Synth synth;
    bool isNoteOn = false;
    float vel = 1;
    double sr;
};
