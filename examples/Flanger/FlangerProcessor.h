#pragma once


#include "lattice/LatticeProcessor.h"
#include "Del.h"
#include "Osc.h"

//===========================================================


class FlangerProcessor : public lattice::Processor {
    
    static float scl(float a, float b) { return a * b; }
	static float lfoFun(double x, const std::vector<float>* nop) {
		return Aurora::cos<float>(x) * 0.46 + 0.54;
	}

	struct Flanger {
		Aurora::Osc<float, lfoFun> lfo;
		Aurora::Del<float, Aurora::vdelayi> delay;
		Aurora::BinOp<float, scl> gain;
		float maxDel;

		Flanger(float maxDt, float sr)
			: lfo(sr), delay(maxDt, sr), gain(), maxDel(maxDt) {}

		const std::vector<float>& operator()(const std::vector<float>& in, float fr,
			float fdb, float g, float maxDel) {
			lfo.vsize(in.size());
			return gain(delay(in, lfo(maxDel, fr), fdb), g);
		}

		void reset(float sr) {
			lfo.reset(sr);
			delay.reset(maxDel, sr);
		}

	};

public:
    // Constructor that initializes the CLAP plugin
    FlangerProcessor();
    
    // Destructor to clean up resources
    ~FlangerProcessor(){};

    // Process method to handle audio processing
    void process(float** inputs, float** outputs, std::size_t blockSize) override;

    // Set a parameter value
    void setParameter(int paramId, double value) override;
    
    // Called whenever the webview sends a message
    void onMessageFromWebView(const nlohmann::json& j) override;
    
    // Called at least once before the processing starts
    void prepareToPlay(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) override;
    
    // Load/Save plugin state
    nlohmann::json savePluginState() override;
    void loadPluginState(nlohmann::json state) override;

    // Create JSON string from parameter data
    nlohmann::json getParameterJson(size_t index, float value);
    
private:
    Flanger flangerLeft, flangerRight;
    std::vector<float> inputLeft, inputRight;
    std::vector<nlohmann::json> webviewMessageQueue;
};
