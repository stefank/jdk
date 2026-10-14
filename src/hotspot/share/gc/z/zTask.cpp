/*
 * Copyright (c) 2017, 2026, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "gc/z/zTask.hpp"
#include "runtime/os.hpp"
#include "utilities/globalDefinitions.hpp"

ZTask::Task::Task(ZTask* task, const char* name)
  : WorkerTask(name),
    _task(task) {}

class ZTask::Task::WorkScope {
private:
  Task* const _task;
  const jlong _start;

public:
  WorkScope(Task* task)
    : _task(task),
      _start(os::current_thread_cpu_time(true /* user + sys */)) {}

  ~WorkScope() {
    const jlong elapsed = os::current_thread_cpu_time(true /* user + sys */) - _start;
    const double vtime = double(elapsed) / NANOSECS_PER_SEC;

    // Accumulate work time
    for (;;) {
      const double prev = _task->_accumulated_vtime.load_relaxed();
      const double new_val = prev + vtime;
      if (_task->_accumulated_vtime.compare_exchange(prev, new_val) == prev) {
        return;
      }
    }
  }
};

void ZTask::Task::work(uint worker_id) {
  WorkScope scope(this);
  _task->work();
}

ZTask::ZTask(const char* name)
  : _worker_task(this, name) {}

const char* ZTask::name() const {
  return _worker_task.name();
}

WorkerTask* ZTask::worker_task() {
  return &_worker_task;
}

void ZTask::clear_accumulated_vtime() {
  _worker_task._accumulated_vtime.store_relaxed(0.0);
}

double ZTask::accumulated_vtime() const {
  return _worker_task._accumulated_vtime.load_relaxed();
}

ZRestartableTask::ZRestartableTask(const char* name)
  : ZTask(name) {}

void ZRestartableTask::resize_workers(uint nworkers) {}
