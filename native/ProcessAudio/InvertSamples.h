#pragma once

#include <cstddef>

inline void InvertAudioSamples(const float *input, float *output, std::size_t count)
{
    for (std::size_t index = 0; index < count; ++index)
        output[index] = -input[index];
}
