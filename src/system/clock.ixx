export module system.clock;

import std;

export class HighResolutionClock
{
public:
    HighResolutionClock();

    // Tick the high resolution clock.
    // Tick the clock before reading the delta time for the first time.
    // Only tick the clock once per frame.
    // Use the Get* functions to return the elapsed time between ticks.
    void Tick();

    // Reset the clock.
    void Reset();

    double GetDeltaNanoseconds() const;
    double GetDeltaMicroseconds() const;
    double GetDeltaMilliseconds() const;
    double GetDeltaSeconds() const;

    double GetTotalNanoseconds() const;
    double GetTotalMicroseconds() const;
    double GetTotalMilliSeconds() const;
    double GetTotalSeconds() const;

private:
    // Initial time point.
    std::chrono::high_resolution_clock::time_point m_T0;
    // Time since last tick.
    std::chrono::high_resolution_clock::duration m_DeltaTime;
    std::chrono::high_resolution_clock::duration m_TotalTime;
};

HighResolutionClock::HighResolutionClock() : m_DeltaTime(0), m_TotalTime(0)
{
    m_T0 = std::chrono::high_resolution_clock::now();
}

void HighResolutionClock::Tick()
{
    auto t1 = std::chrono::high_resolution_clock::now();
    m_DeltaTime = t1 - m_T0;
    m_TotalTime += m_DeltaTime;
    m_T0 = t1;
}

void HighResolutionClock::Reset()
{
    m_T0 = std::chrono::high_resolution_clock::now();
    m_DeltaTime = std::chrono::high_resolution_clock::duration();
    m_TotalTime = std::chrono::high_resolution_clock::duration();
}

double HighResolutionClock::GetDeltaNanoseconds() const
{
    return m_DeltaTime.count() * 1.0;
}
double HighResolutionClock::GetDeltaMicroseconds() const
{
    return m_DeltaTime.count() * 1e-3;
}

double HighResolutionClock::GetDeltaMilliseconds() const
{
    return m_DeltaTime.count() * 1e-6;
}

double HighResolutionClock::GetDeltaSeconds() const
{
    return m_DeltaTime.count() * 1e-9;
}

double HighResolutionClock::GetTotalNanoseconds() const
{
    return m_TotalTime.count() * 1.0;
}

double HighResolutionClock::GetTotalMicroseconds() const
{
    return m_TotalTime.count() * 1e-3;
}

double HighResolutionClock::GetTotalMilliSeconds() const
{
    return m_TotalTime.count() * 1e-6;
}

double HighResolutionClock::GetTotalSeconds() const
{
    return m_TotalTime.count() * 1e-9;
}
