#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

/*
 * this could implement different parallel execution strategies,
 * currently it only supports the POSIX jobserver
 */

namespace parallel {

/*
 * return true if the jobserver appears to be available, otherwise
 * returns false, in which case none of the other functions should be
 * called
 */
bool init(int _max_processes);

/*
 * called from parent, like fork() returns non-zero to parent and zero
 * to child; does not fork until max_processes is respected and,
 * additionally, blocks the child until a jobserver token is available
 */
int limitedFork();

/*
 * called from a child that has finished executing
 */
[[noreturn]] void finishChild();

/*
 * called from parent, returns when all child processes have
 * terminated
 */
void waitForAllChildren();

} // namespace parallel
