#include "../../native/ProcessAudio/InvertSamples.h"

#include <array>
#include <cassert>
#include <cmath>

int main()
{
    constexpr std::array<float, 6> system{0.1f, -0.2f, 0, 0.3f, -0.4f, 0};
    constexpr std::array<float, 6> app1{0.2f, 0.4f, 0, -0.1f, -0.3f, 0};
    constexpr std::array<float, 6> app2{-0.1f, 0.1f, 0, 0.2f, 0.1f, 0};
    std::array<float, 6> inverse1{}, inverse2{};
    InvertAudioSamples(app1.data(), inverse1.data(), app1.size());
    InvertAudioSamples(app2.data(), inverse2.data(), app2.size());
    for (std::size_t index = 0; index < system.size(); ++index) {
        const float residual = system[index] + app1[index] + app2[index] +
                               inverse1[index] + inverse2[index];
        assert(std::abs(residual - system[index]) < 0.000001f);
        for (float gain : {0.0f, 0.37f, 1.0f})
            assert(std::abs(residual * gain - system[index] * gain) < 0.000001f);
        assert(inverse1[index] == -app1[index]);
    }
}
