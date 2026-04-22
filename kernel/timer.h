#pragma once

#include "drivers/io.h"

// Timer states
enum TimerState {
    TIMER_STOPPED = 0,
    TIMER_RUNNING = 1,
    TIMER_FIRED = 2
};

// PIT (Programmable Interval Timer) ports and constants
static constexpr unsigned short PIT_CHANNEL_0 = 0x40;
static constexpr unsigned short PIT_COMMAND = 0x43;
static constexpr unsigned int PIT_FREQUENCY = 1193182;  // 1.193182 MHz

// Timer object structure
struct TimerObject {
    unsigned long long interval_ms;     // Interval in milliseconds
    unsigned long long elapsed_ms;      // Time elapsed since start
    unsigned long long fire_count;      // Number of times fired
    TimerState state;
    bool auto_reload;                   // Auto-reload on expiration
    void (*callback)(void);             // Optional callback function
    unsigned int id;                    // Timer identifier

    TimerObject() : interval_ms(0), elapsed_ms(0), fire_count(0), 
                    state(TIMER_STOPPED), auto_reload(false), 
                    callback(nullptr), id(0) {}
};

// System Timer Manager
class SystemTimer {
private:
    static constexpr int MAX_TIMERS = 32;
    TimerObject timers[MAX_TIMERS];
    int timer_count;
    unsigned int next_id;
    unsigned long long tick_count;

public:
    SystemTimer() : timer_count(0), next_id(1), tick_count(0) {}

    // Initialize the timer system with desired frequency (Hz)
    void init(unsigned int frequency = 100) {
        tick_count = 0;
        timer_count = 0;
        next_id = 1;

        // Calculate divisor for desired frequency
        unsigned short divisor = (unsigned short)(PIT_FREQUENCY / frequency);

        // Set PIT channel 0 to mode 3 (square wave), binary mode
        outb(PIT_COMMAND, 0x36);  // 0x36 = channel 0, both bytes, mode 3
        outb(PIT_CHANNEL_0, divisor & 0xFF);
        outb(PIT_CHANNEL_0, (divisor >> 8) & 0xFF);
    }

    // Create and register a new timer
    unsigned int create_timer(unsigned long long interval_ms, bool auto_reload = false) {
        if (timer_count >= MAX_TIMERS)
            return 0;  // Failed

        TimerObject& timer = timers[timer_count++];
        timer.interval_ms = interval_ms;
        timer.elapsed_ms = 0;
        timer.fire_count = 0;
        timer.state = TIMER_STOPPED;
        timer.auto_reload = auto_reload;
        timer.id = next_id;

        return next_id++;
    }

    // Start a timer by ID
    bool start_timer(unsigned int timer_id) {
        for (int i = 0; i < timer_count; i++) {
            if (timers[i].id == timer_id) {
                timers[i].state = TIMER_RUNNING;
                timers[i].elapsed_ms = 0;
                return true;
            }
        }
        return false;
    }

    // Stop a timer by ID
    bool stop_timer(unsigned int timer_id) {
        for (int i = 0; i < timer_count; i++) {
            if (timers[i].id == timer_id) {
                timers[i].state = TIMER_STOPPED;
                return true;
            }
        }
        return false;
    }

    // Cancel/delete a timer by ID
    bool cancel_timer(unsigned int timer_id) {
        for (int i = 0; i < timer_count; i++) {
            if (timers[i].id == timer_id) {
                // Swap with last timer and reduce count
                timers[i] = timers[timer_count - 1];
                timer_count--;
                return true;
            }
        }
        return false;
    }

    // Set callback function for a timer
    bool set_callback(unsigned int timer_id, void (*callback)(void)) {
        for (int i = 0; i < timer_count; i++) {
            if (timers[i].id == timer_id) {
                timers[i].callback = callback;
                return true;
            }
        }
        return false;
    }

    // Query timer status
    bool query_timer(unsigned int timer_id, unsigned long long& elapsed_ms, TimerState& state) {
        for (int i = 0; i < timer_count; i++) {
            if (timers[i].id == timer_id) {
                elapsed_ms = timers[i].elapsed_ms;
                state = timers[i].state;
                return true;
            }
        }
        return false;
    }

    // Update all timers (called from system tick/interrupt)
    void tick(unsigned long long ms_elapsed = 1) {
        tick_count += ms_elapsed;

        for (int i = 0; i < timer_count; i++) {
            if (timers[i].state != TIMER_RUNNING)
                continue;

            timers[i].elapsed_ms += ms_elapsed;

            if (timers[i].elapsed_ms >= timers[i].interval_ms) {
                timers[i].fire_count++;
                timers[i].state = TIMER_FIRED;

                // Execute callback if set
                if (timers[i].callback)
                    timers[i].callback();

                // Handle auto-reload
                if (timers[i].auto_reload) {
                    timers[i].elapsed_ms = 0;
                    timers[i].state = TIMER_RUNNING;
                } else {
                    timers[i].state = TIMER_STOPPED;
                }
            }
        }
    }

    // Get total system ticks
    unsigned long long get_ticks() const {
        return tick_count;
    }

    // Get number of active timers
    int get_timer_count() const {
        return timer_count;
    }
};
