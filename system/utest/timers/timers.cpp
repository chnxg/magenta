// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdio.h>
#include <threads.h>

#include <mx/timer.h>

#include <mxtl/type_support.h>

#include <unistd.h>
#include <unittest/unittest.h>

static bool basic_test() {
    BEGIN_TEST;
    mx::timer timer;
    ASSERT_EQ(mx::timer::create(0, MX_CLOCK_MONOTONIC, &timer), MX_OK);

    mx_signals_t pending;
    EXPECT_EQ(timer.wait_one(MX_TIMER_SIGNALED, 0u, &pending), MX_ERR_TIMED_OUT);
    EXPECT_EQ(pending, MX_SIGNAL_LAST_HANDLE);

    for (int ix = 0; ix != 10; ++ix) {
        const auto deadline_timer = mx_deadline_after(MX_MSEC(50));
        const auto deadline_wait = mx_deadline_after(MX_SEC(1));
        // Timer should fire faster than the wait timeout.
        ASSERT_EQ(timer.start(deadline_timer, 0u, 0u), MX_OK);
        EXPECT_EQ(timer.wait_one(MX_TIMER_SIGNALED, deadline_wait, &pending), MX_OK);
        EXPECT_EQ(pending, MX_TIMER_SIGNALED | MX_SIGNAL_LAST_HANDLE);
    }
    END_TEST;
}

static bool restart_test() {
    BEGIN_TEST;
    mx::timer timer;
    ASSERT_EQ(mx::timer::create(0, MX_CLOCK_MONOTONIC, &timer), MX_OK);

    mx_signals_t pending;
    for (int ix = 0; ix != 10; ++ix) {
        const auto deadline_timer = mx_deadline_after(MX_MSEC(500));
        const auto deadline_wait = mx_deadline_after(MX_MSEC(1));
        // Setting a timer already running is equivalent to a cancel + set.
        ASSERT_EQ(timer.start(deadline_timer, 0u, 0u), MX_OK);
        EXPECT_EQ(timer.wait_one(MX_TIMER_SIGNALED, deadline_wait, &pending), MX_ERR_TIMED_OUT);
        EXPECT_EQ(pending, MX_SIGNAL_LAST_HANDLE);
    }
    END_TEST;
}

static bool invalid_calls() {
    BEGIN_TEST;

    mx::timer timer;
    ASSERT_EQ(mx::timer::create(0, MX_CLOCK_UTC, &timer), MX_ERR_INVALID_ARGS);
    ASSERT_EQ(mx::timer::create(1, MX_CLOCK_MONOTONIC, &timer), MX_ERR_INVALID_ARGS);

    ASSERT_EQ(mx::timer::create(0, MX_CLOCK_MONOTONIC, &timer), MX_OK);
    ASSERT_EQ(timer.start(0u, 0u, 0u), MX_ERR_INVALID_ARGS);
    ASSERT_EQ(timer.start(MX_TIMER_MIN_DEADLINE - 1, 0u, 0u), MX_ERR_INVALID_ARGS);

    const auto deadline_timer = mx_deadline_after(MX_MSEC(1));
    ASSERT_EQ(timer.start(deadline_timer, MX_USEC(2), 0u), MX_ERR_NOT_SUPPORTED);

    END_TEST;
}

static bool edge_cases() {
    BEGIN_TEST;

    mx::timer timer;
    ASSERT_EQ(mx::timer::create(0, MX_CLOCK_MONOTONIC, &timer), MX_OK);
    ASSERT_EQ(timer.start(MX_TIMER_MIN_DEADLINE, 0u, 0u), MX_OK);
    ASSERT_EQ(timer.start(MX_TIMER_MIN_DEADLINE, MX_TIMER_MIN_PERIOD, 0u), MX_OK);

    END_TEST;
}

static bool periodic() {
    BEGIN_TEST;

    mx::timer timer;
    ASSERT_EQ(mx::timer::create(0, MX_CLOCK_MONOTONIC, &timer), MX_OK);

    const auto deadline_timer = mx_deadline_after(MX_MSEC(1));
    const auto period = MX_USEC(500);

    ASSERT_EQ(timer.start(deadline_timer, period, 0u), MX_OK);

    mx_signals_t pending;
    auto expected_arrival = deadline_timer;

    for (int ix = 0; ix != 100; ++ix) {
        EXPECT_EQ(timer.wait_one(MX_TIMER_SIGNALED, MX_TIME_INFINITE, &pending), MX_OK);
        EXPECT_EQ(pending & MX_TIMER_SIGNALED, MX_TIMER_SIGNALED);

        EXPECT_GT(mx_time_get(MX_CLOCK_MONOTONIC), expected_arrival);
        expected_arrival += period;
    }

    EXPECT_EQ(timer.cancel(), MX_OK);
    END_TEST;
}

// furiously spin resetting the timer, trying to race with it going off to look for
// race conditions.
static bool restart_race() {
    BEGIN_TEST;

    const mx_time_t kTestDuration = MX_SEC(5);
    auto start = mx_time_get(MX_CLOCK_MONOTONIC);

    mx::timer timer;
    ASSERT_EQ(mx::timer::create(0, MX_CLOCK_MONOTONIC, &timer), MX_OK);
    while (mx_time_get(MX_CLOCK_MONOTONIC) - start < kTestDuration) {
        ASSERT_EQ(timer.start(MX_TIMER_MIN_DEADLINE, MX_TIMER_MIN_PERIOD, 0u), MX_OK);
    }

    EXPECT_EQ(timer.cancel(), MX_OK);

    END_TEST;
}

// If the timer is already due at the moment it is started then the signal should be
// asserted immediately.  Likewise canceling the timer should immediately deassert
// the signal.
static bool signals_asserted_immediately() {
    BEGIN_TEST;
    mx::timer timer;
    ASSERT_EQ(mx::timer::create(0, MX_CLOCK_MONOTONIC, &timer), MX_OK);

    for (int i = 0; i < 100; i++) {
        mx_time_t now = mx_time_get(MX_CLOCK_MONOTONIC);
        EXPECT_EQ(timer.start(now, 0u, 0u), MX_OK);

        mx_signals_t pending;
        EXPECT_EQ(timer.wait_one(MX_TIMER_SIGNALED, 0u, &pending), MX_OK);
        EXPECT_EQ(pending, MX_TIMER_SIGNALED | MX_SIGNAL_LAST_HANDLE);

        EXPECT_EQ(timer.cancel(), MX_OK);

        EXPECT_EQ(timer.wait_one(MX_TIMER_SIGNALED, 0u, &pending), MX_ERR_TIMED_OUT);
        EXPECT_EQ(pending, MX_SIGNAL_LAST_HANDLE);
    }

    END_TEST;
}

BEGIN_TEST_CASE(timers_test)
RUN_TEST(basic_test)
RUN_TEST(restart_test)
RUN_TEST(invalid_calls)
RUN_TEST(edge_cases)
RUN_TEST(periodic)
RUN_TEST(restart_race)
RUN_TEST(signals_asserted_immediately)
END_TEST_CASE(timers_test)

int main(int argc, char** argv) {
    bool success = unittest_run_all_tests(argc, argv);
    return success ? 0 : -1;
}
