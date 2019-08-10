#pragma once

#include <ostream>

class TimerLog {
protected:
    static TimerLog *gTimerLog;

    template<typename T>
    void Log(const char* name, T us) {
        mStream << "[" << name << "] Performed in: " << us << "us | " << (us * .001) << "ms" << std::endl;
    }

private:
    std::ostream &mStream;

public:
    TimerLog();

    static void SetTimerLog(TimerLog* inst) { gTimerLog = inst; }


};