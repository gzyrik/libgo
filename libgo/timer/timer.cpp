#include "timer.h"
#include "../scheduler/scheduler.h"
#include "../scheduler/processer.h"
#include "../libgo.h"

namespace co
{

CoTimer::CoTimerImpl::CoTimerImpl(FastSteadyClock::duration precision)
    : precision_(precision)
{
//    trigger_.SetDbgMask(0);
}

CoTimer::CoTimerImpl::~CoTimerImpl()
{

}

void CoTimer::CoTimerImpl::RunInCoroutine()
{
    while (!terminate_) {
        DebugPrint(dbg_timer, "trigger RunOnce");
        RunOnce();

        if (terminate_) break;

        std::unique_lock<LFLock> lock(lock_);

        auto nextTime = NextTrigger(precision_);
        auto now = FastSteadyClock::now();
        auto nextDuration = nextTime > now ? nextTime - now : FastSteadyClock::duration(0);
        DebugPrint(dbg_timer, "wait trigger nextDuration=%d ns", (int)std::chrono::duration_cast<std::chrono::nanoseconds>(nextDuration).count());
        if (nextDuration.count() > 0) {
            trigger_.TimedPop(nullptr, nextDuration);
        } else {
            trigger_.TryPop(nullptr);
        }
    }
}

void CoTimer::CoTimerImpl::Stop()
{
    terminate_ = true;
}

CoTimer::CoTimerImpl::TimerId
CoTimer::CoTimerImpl::ExpireAt(FastSteadyClock::duration dur, func_t const& cb)
{
    DebugPrint(dbg_timer, "add timer dur=%d", (int)std::chrono::duration_cast<std::chrono::milliseconds>(dur).count());

    auto id = StartTimer(dur, cb);

    // 强制唤醒, 提高精准度
    if (dur <= precision_) {
        std::unique_lock<LFLock> lock(lock_, std::defer_lock);
        if (lock.try_lock()) return id;

        trigger_.TryPush(nullptr);
    }

    return id;
}

void CoTimer::Initialize(Scheduler * scheduler)
{
    auto ptr = impl_;
    go co_scheduler(scheduler) [ptr] {
        ptr->RunInCoroutine();
    };
}

CoTimer::~CoTimer()
{
    impl_->Stop();
}

CoTimer::TimerId CoTimer::ExpireAt(FastSteadyClock::duration dur, func_t const& cb)
{
    return impl_->ExpireAt(dur, cb);
}

CoTimer::TimerId CoTimer::ExpireAt(FastSteadyClock::time_point tp, func_t const& cb)
{
    auto now = FastSteadyClock::now();
    auto dur = (tp > now) ? tp - now : FastSteadyClock::duration(0);
    return ExpireAt(dur, cb);
}

} //namespace co
