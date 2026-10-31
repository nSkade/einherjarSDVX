
#include <thread>
#include <chrono>

#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

namespace ehj {

/**
 * @brief CPU based wait until target Time is reached
 * see GLFWfpsLimiter for fetching automatic refreshrate
*/
class FPSLimiter {
public:
	FPSLimiter(uint32_t fpsLimit = 73) {
		m_fpsLimit = fpsLimit; 
		calcInterval();
		m_tp = std::chrono::high_resolution_clock::now();
	};
	void setLimit(uint32_t fpsLimit) { m_fpsLimit = fpsLimit; calcInterval(); };
	
	void wait() {
#if 0 // active spinlock // < 1ms deviation
		auto target_tp = m_tp + std::chrono::microseconds(m_interval);
		auto now = std::chrono::steady_clock::now();

		if (now > target_tp) {
			m_tp = now;
		} else {
			while (std::chrono::steady_clock::now() < target_tp) {}
			m_tp = target_tp;
		}
#endif
#if 1 // unleashed recompiled ref

#ifdef _WIN32
		{ // enable lower windows scedule time resolution
			static bool enabled = false;
			if (!enabled) {
				timeBeginPeriod(1);
				enabled=true;
			}
		}
#endif
		auto now = std::chrono::steady_clock::now();

		if (now < m_tp){ 
			std::this_thread::sleep_for(std::chrono::floor<std::chrono::milliseconds>(m_tp - now - std::chrono::milliseconds(2)));

			while ((now = std::chrono::steady_clock::now()) < m_tp)
				std::this_thread::yield();
		}
		else
			m_tp = now;

		m_tp += std::chrono::nanoseconds(1000000000) / m_fpsLimit;
#endif
	}
	
	void wait(std::function<void(double)> sleepFunc) {
		auto dt = std::chrono::steady_clock::now()-m_tp;
		auto overhead = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::microseconds(m_interval) - dt);
		if (overhead.count() > 0)
			sleepFunc(overhead.count());
		m_tp = std::chrono::steady_clock::now();
	}

private:
	void calcInterval() {
		m_interval = (double) 1.0/m_fpsLimit*1000.0*1000.0;
	}
	std::chrono::steady_clock::time_point m_tp;
	uint32_t m_fpsLimit = 73;
	uint32_t m_interval;
};

}//ehj
