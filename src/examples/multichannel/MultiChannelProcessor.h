// CawProcessor.h
#pragma once

#include <cstddef> // for std::size_t
#include <vector>
#include "lattice/clap/ClapPlugin.h" // Include the necessary CLAP headers
#include "lattice/LatticeProcessor.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//===========================================================


class MultiChannelProcessor : public lattice::Processor {
    
public:
    // Constructor that initializes the CLAP plugin
    MultiChannelProcessor();
    
    // Destructor to clean up resources
    ~MultiChannelProcessor(){};

    // Process method to handle audio processing
    void process(float** inputs, float** outputs, std::size_t blockSize) override;

    // Set a parameter value
    void setParameter(int paramId, double value) override;
    
    // Called whenever the webview sends a message
    void onMesssgeFromWebView(nlohmann::json j) override;
    
    // Called at least once before the processing starts
    void prepareToPlay(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) override;
    
private:
    // Number of audio inputs and outputs
    int numInputs = 0;
    int numOutputs = 0;
};
