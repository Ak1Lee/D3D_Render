#pragma once

#include <chrono>

class Timer
{
public:
	Timer();
	Timer(const Timer&) = delete;
	Timer& operator=(const Timer&) = delete;

	const double GetTotalTime(); // in seconds

	double GetDeltaTime() const; // in seconds

	double GetDeltaTimeAndUpdateLastTime();

	void UpdateLastTime();

	void SetLastTime(float CurrentTime);

private:
	std::chrono::high_resolution_clock::time_point StartTime;
	std::chrono::high_resolution_clock::time_point LastTime;
};