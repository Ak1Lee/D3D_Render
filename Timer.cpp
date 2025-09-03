#include "Timer.h"

Timer::Timer()
{
	StartTime = std::chrono::high_resolution_clock::now();
	LastTime = StartTime;
}

double Timer::GetTotalTime()
{
	
	LastTime = std::chrono::high_resolution_clock::now();
	return std::chrono::duration<double>(LastTime - StartTime).count();
}

double Timer::GetDeltaTime() const
{
	auto CurrentTime = std::chrono::high_resolution_clock::now();
	auto DeltaTime = std::chrono::duration<double>(CurrentTime - LastTime).count();
	return DeltaTime;
}

double Timer::GetDeltaTimeAndUpdateLastTime()
{
	auto CurrentTime = std::chrono::high_resolution_clock::now();
	auto DeltaTime = std::chrono::duration<double>(CurrentTime - LastTime).count();
	LastTime = CurrentTime;
	return DeltaTime;

}

void Timer::UpdateLastTime()
{
	auto CurrentTime = std::chrono::high_resolution_clock::now();
	LastTime = CurrentTime;
}

void Timer::SetLastTime(float CurrentTime)
{
	UpdateLastTime();
}
