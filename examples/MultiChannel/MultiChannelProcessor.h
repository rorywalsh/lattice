#pragma once

#include "lattice/LatticeProcessor.h"

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
    void onMessageFromWebView(const nlohmann::json &j) override;
    
    // Called at least once before the processing starts
    void prepareToPlay(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) override;
    
private:

};
