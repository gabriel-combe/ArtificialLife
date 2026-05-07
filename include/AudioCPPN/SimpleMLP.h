#pragma once
#include <Eigen/Dense>
#include <vector>
#include <random>
#include <cmath>

namespace AudioCPPN {

// Feedforward MLP: no bias, Tanh on every layer including output.
// Weights are randomly initialised from N(0, scale).
class SimpleMLP {
public:
    SimpleMLP(int inputSize, int outputSize, int hiddenSize, int layerCount);

    // Forward pass — returns outputSize values in (-1, 1).
    Eigen::VectorXf forward(const Eigen::VectorXf& input) const;

    // Re-initialise all weights from N(0, scale). seed=0 uses std::random_device.
    void randomiseWeights(float scale = 1.f, unsigned seed = 0);

    // Rebuild the network with new dimensions, then randomise weights.
    void rebuild(int inputSize, int outputSize, int hiddenSize, int layerCount,
                 float scale = 1.f, unsigned seed = 0);

    int getInputSize()  const { return inputSize_;  }
    int getOutputSize() const { return outputSize_; }
    int getHiddenSize() const { return hiddenSize_; }
    int getLayerCount() const { return (int)weights_.size(); }

    // Flatten all weights into a single vector (for serialization).
    std::vector<float> getWeightsFlat() const;
    // Restore weights from a flat vector (must match current network topology).
    void setWeightsFlat(const std::vector<float>& flat);

private:
    int inputSize_;
    int outputSize_;
    int hiddenSize_;

    // weights_[0]     : (hiddenSize x inputSize)
    // weights_[1..n-2]: (hiddenSize x hiddenSize)
    // weights_[n-1]   : (outputSize x hiddenSize)
    std::vector<Eigen::MatrixXf> weights_;

    void allocateLayers(int inputSize, int outputSize, int hiddenSize, int layerCount);
};

} // namespace AudioCPPN
