#include "Timer.h"
#include "MMU.h"

Timer::Timer(MMU& mmu) : mmu(mmu) {
    reset();
}

void Timer::reset() {
    counter.fill(0);
    reload.fill(0);
    control.fill(0);
    prescalerCounter.fill(0);
}

void Timer::step(int cycles) {
    for (int i = 0; i < 4; i++) {
        if (!(control[i] & 0x80)) continue;
        
        bool cascade = (control[i] & 0x04) && (i > 0);
        if (cascade) continue;
        
        int prescaler = prescalerShifts[control[i] & 0x03];
        prescalerCounter[i] += cycles;
        
        int ticksNeeded = 1 << prescaler;
        while (prescalerCounter[i] >= ticksNeeded) {
            prescalerCounter[i] -= ticksNeeded;
            tick(i);
        }
    }
}

void Timer::tick(int timer) {
    counter[timer]++;
    
    if (counter[timer] == 0) {
        overflow(timer);
    }
}

void Timer::overflow(int timer) {
    counter[timer] = reload[timer];
    
    if (control[timer] & 0x40) {
        uint16_t if_ = mmu.getIF();
        mmu.setIF(if_ | (1 << (3 + timer)));
    }
    
    if (timer < 3) {
        int next = timer + 1;
        bool nextCascade = (control[next] & 0x04) && (control[next] & 0x80);
        if (nextCascade) {
            tick(next);
        }
    }
}

uint16_t Timer::readCounter(int timer) const {
    if (timer < 0 || timer >= 4) return 0;
    return counter[timer];
}

uint16_t Timer::readControl(int timer) const {
    if (timer < 0 || timer >= 4) return 0;
    return control[timer];
}

void Timer::writeReload(int timer, uint16_t value) {
    if (timer < 0 || timer >= 4) return;
    reload[timer] = value;
}

void Timer::writeControl(int timer, uint16_t value) {
    if (timer < 0 || timer >= 4) return;
    
    bool wasEnabled = control[timer] & 0x80;
    bool nowEnabled = value & 0x80;
    
    if (!wasEnabled && nowEnabled) {
        counter[timer] = reload[timer];
        prescalerCounter[timer] = 0;
    }
    
    control[timer] = value;
}

constexpr int Timer::prescalerShifts[4];
