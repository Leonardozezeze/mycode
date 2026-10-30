#include "FilterEngine.h"
#include <algorithm>
#include <numeric>

FilterEngine::FilterEngine() = default;

void FilterEngine::setFilterType(FilterType type)
{
    m_type = type;
    reset();
}

void FilterEngine::setWindowSize(int size)
{
    m_windowSize = std::max(1, size);
    reset();
}

void FilterEngine::setEmaAlpha(double alpha)
{
    m_emaAlpha = std::clamp(alpha, 0.0, 1.0);
    reset();
}

double FilterEngine::process(double input)
{
    switch (m_type) {
    case FilterType::MovingAverage: {
        m_buffer.push_back(input);
        while (static_cast<int>(m_buffer.size()) > m_windowSize) {
            m_buffer.pop_front();
        }
        double sum = std::accumulate(m_buffer.begin(), m_buffer.end(), 0.0);
        return sum / static_cast<double>(m_buffer.size());
    }
    case FilterType::EMA: {
        if (!m_emaInitialized) {
            m_emaPrev = input;
            m_emaInitialized = true;
            return input;
        }
        double out = m_emaAlpha * input + (1.0 - m_emaAlpha) * m_emaPrev;
        m_emaPrev = out;
        return out;
    }
    case FilterType::None:
    default:
        return input;
    }
}

void FilterEngine::reset()
{
    m_buffer.clear();
    m_emaPrev = 0.0;
    m_emaInitialized = false;
}
