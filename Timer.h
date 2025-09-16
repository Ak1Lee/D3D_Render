#pragma once

#include <chrono>
#include <unordered_map>

struct DeltaTimeInfo
{
	float StartTime = 0.0f;
	float LastTime = 0.0f;
	float DeltaTime = 0.0f;
	float TotalTime = 0.0f;
};

class Timer
{
public:

	Timer(const Timer&) = delete;

	double GetTotalTime(); // in seconds

	double GetDeltaTime() const; // in seconds

	double GetDeltaTimeAndUpdateLastTime();

	void UpdateLastTime();

	void SetLastTime(float CurrentTime);

	void PrintTimeMap();

	void StartNamedTimer(const std::string& Name);

	void StopNamedTimer(const std::string& Name);

	static Timer& GetTimerInstance();


private:

	Timer();

	std::chrono::high_resolution_clock::time_point StartTime;
	std::chrono::high_resolution_clock::time_point LastTime;

	std::unordered_map<std::string, DeltaTimeInfo> NamedDeltaTimeMap;
};