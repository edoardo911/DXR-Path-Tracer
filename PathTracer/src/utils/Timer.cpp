#include "Timer.h"

namespace RT
{
	Timer::Timer(): mSecondsPerCount(0.0), mDeltaTime(-1.0), mBaseTime(0), mPausedTime(0), mPrevTime(0), mCurrTime(0), mStopped(false)
	{
		__int64 countsPerSec;
		QueryPerformanceFrequency((LARGE_INTEGER*) &countsPerSec);
		mSecondsPerCount = 1.0 / (double) countsPerSec;
	}

	void Timer::stop()
	{
		if(!mStopped)
		{
			__int64 currTime;
			QueryPerformanceCounter((LARGE_INTEGER*) &currTime);

			mStopTime = currTime;
			mStopped = true;
		}
	}

	void Timer::start()
	{
		__int64 startTime;
		QueryPerformanceCounter((LARGE_INTEGER*) &startTime);

		if(mStopped)
		{
			mPausedTime += (startTime - mStopTime);
			mPrevTime = startTime;
			mStopTime = 0;
			mStopped = false;
		}
	}

	float Timer::totalTime() const
	{
		if(mStopped)
			return (float) (((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
		else
			return (float) (((mCurrTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
	}

	void Timer::tick()
	{
		if(mStopped)
		{
			mDeltaTime = 0.0;
			return;
		}

		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*) &currTime);
		mCurrTime = currTime;

		mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;
		mPrevTime = mCurrTime;

		if(mDeltaTime < 0.0)
			mDeltaTime = 0.0;
	}

	float Timer::deltaTime() const { return (float) mDeltaTime; }

	void Timer::reset()
	{
		__int64 currTime;
		QueryPerformanceCounter((LARGE_INTEGER*) &currTime);

		mBaseTime = currTime;
		mPrevTime = currTime;
		mStopTime = 0;
		mStopped = false;
	}
}