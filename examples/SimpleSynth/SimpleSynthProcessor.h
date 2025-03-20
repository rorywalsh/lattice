// CawProcessor.h
#pragma once

#include "lattice/LatticeProcessor.h"
#include "BlOsc.h"
#include "Env.h"

//===========================================================


class SimpleSynthProcessor : public lattice::Processor 
{
    /* Basic synth class that contains Aurora::Env, Aurora::BlOsc,
       and Aurora::TableSet objects */
    class Synth {

    public:
        Synth(int noteNumber, float rt, float sr);
        void setWaveform(int waveForm);
        void setBlockSize(std::size_t blockSize);
        void setSampleRate(int sr);


        const std::vector<float>& operator()(float a, float f, bool gate,
            std::size_t vsiz = 0) 
        {
            if (vsiz)
                osc.vsize(vsiz);
            return env(osc(a, f), gate);
        }

        void setAttack(float value) { att = value; }
        void setDecay(float value) { dec = value; }
        void setSustain(float value) { sus = value; }
        void setRelease(float value) { env.release(value);   rel = value; }
        float getAttack() { return att; }
        float getDecay() { return dec; }
        float getSustain() { return sus; }
        float getRelease() { return rel; }
		int getNoteNumber() { return midiNoteNumber; }
		void setNoteNumber(int value) { midiNoteNumber = value; }
		void setNoteType(bool value) { noteType = value; }
		bool getNoteType() { return noteType; }

    private:
        float att, dec, sus, rel;
        int midiNoteNumber = 0;
		bool noteType = false;
        std::vector<float> wave;
        //Aurora::BlOsc<float> osc;
        Aurora::Osc<float, Aurora::lookupi<float>> osc;
        Aurora::Env<float> env;
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
    void process(float** inputs, float** outputs, std::size_t blockSize) override;

    // Set a parameter value
    void setParameter(int paramId, double value) override;
    
    // Called whenever the webview sends a message
    void onMesssgeFromWebView(const nlohmann::json &j) override;
    
    // Called at least once before the processing starts
    void prepareToPlay(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) override;
    
    

private:
    void manageVoices(lattice::NoteEvent noteEvent);
    std::vector<SimpleSynthProcessor::Synth> synthVoices;
    bool isNoteOn = false;
    float vel = 1;
    double sr;
};
