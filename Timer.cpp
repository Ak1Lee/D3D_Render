#include "Timer.h"
#include <iostream>

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

void Timer::PrintTimeMap()
{
	for(auto & Pair : NamedDeltaTimeMap)
	{
		std::cout << "Name: " << Pair.first 
			//<< " StartTime: " << Pair.second.StartTime
			//<< " LastTime: " << Pair.second.LastTime
			<< " DeltaTime: " << Pair.second.DeltaTime
			//<< " TotalTime: " << Pair.second.TotalTime
			<< std::endl;
	}
}

void Timer::StartNamedTimer(const std::string& Name)
{
	auto CurrentTime = std::chrono::high_resolution_clock::now();
	float TimeInSeconds = std::chrono::duration<float>(CurrentTime - StartTime).count();
	auto It = NamedDeltaTimeMap.find(Name);
	if(It == NamedDeltaTimeMap.end())
	{
		DeltaTimeInfo Info;
		Info.StartTime = TimeInSeconds;
		Info.LastTime = TimeInSeconds;
		Info.TotalTime = 0.0f;
		Info.DeltaTime = 0.0f;
		NamedDeltaTimeMap[Name] = Info;
	}
	else
	{
		It->second.StartTime = TimeInSeconds;
		It->second.LastTime = TimeInSeconds;
	}
}

void Timer::StopNamedTimer(const std::string& Name)
{
	auto CurrentTime = std::chrono::high_resolution_clock::now();
	float TimeInSeconds = std::chrono::duration<float>(CurrentTime - StartTime).count();

	if(NamedDeltaTimeMap.find(Name) != NamedDeltaTimeMap.end())
	{
		auto& Info = NamedDeltaTimeMap[Name];
		float DeltaTime = TimeInSeconds - Info.LastTime;
		Info.TotalTime += DeltaTime;
		Info.LastTime = TimeInSeconds;
		Info.DeltaTime = DeltaTime;
	}
}

Timer& Timer::GetTimerInstance()
{
	// TODO: 在此处插入 return 语句
	{
		static Timer Instance;
		return Instance;
	}
}
