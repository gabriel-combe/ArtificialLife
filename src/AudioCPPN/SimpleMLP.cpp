#include "AudioCPPN/SimpleMLP.h"
#include <stdexcept>

namespace AudioCPPN {

SimpleMLP::SimpleMLP(int inputSize, int outputSize, int hiddenSize, int layerCount) {
    allocateLayers(inputSize, outputSize, hiddenSize, layerCount);
    randomiseWeights(1.f, 0);
}

void SimpleMLP::allocateLayers(int in, int out, int hidden, int layers) {
    if (layers < 2)
        throw std::invalid_argument("SimpleMLP requires at least 2 layers");
    inputSize_  = in;
    outputSize_ = out;
    hiddenSize_ = hidden;
    weights_.resize(layers);
    weights_[0] = Eigen::MatrixXf(hidden, in);
    for (int l = 1; l < layers - 1; ++l)
        weights_[l] = Eigen::MatrixXf(hidden, hidden);
    weights_[layers - 1] = Eigen::MatrixXf(out, hidden);
}

void SimpleMLP::randomiseWeights(float scale, unsigned seed) {
    std::mt19937 rng(seed == 0 ? std::random_device{}() : seed);
    for (auto& W : weights_) {
        // Xavier init for tanh: std = scale / sqrt(fan_in)
        // Keeps activations in the linear regime regardless of depth/width,
        // so hidden size and layer count have a real effect.
        float std = scale / std::sqrt((float)W.cols());
        std::normal_distribution<float> dist(0.f, std);
        for (int i = 0; i < W.rows(); ++i)
            for (int j = 0; j < W.cols(); ++j)
                W(i, j) = dist(rng);
    }
}

void SimpleMLP::rebuild(int in, int out, int hidden, int layers,
                        float scale, unsigned seed) {
    allocateLayers(in, out, hidden, layers);
    randomiseWeights(scale, seed);
}

std::vector<float> SimpleMLP::getWeightsFlat() const {
    std::vector<float> flat;
    for (const auto& W : weights_)
        for (int i = 0; i < (int)W.size(); ++i)
            flat.push_back(W.data()[i]);
    return flat;
}

void SimpleMLP::setWeightsFlat(const std::vector<float>& flat) {
    int pos = 0;
    for (auto& W : weights_)
        for (int i = 0; i < (int)W.size() && pos < (int)flat.size(); ++i)
            W.data()[i] = flat[pos++];
}

Eigen::VectorXf SimpleMLP::forward(const Eigen::VectorXf& x) const {
    Eigen::VectorXf h = (weights_[0] * x).array().tanh();
    for (int l = 1; l < (int)weights_.size(); ++l)
        h = (weights_[l] * h).array().tanh();
    return h;
}

} // namespace AudioCPPN
