/*
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates. All rights reserved.
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

/**
 * @test
 * @summary Stress test that reaches the process limit for thread count, or time limit.
 * @key stress
 * @library /test/lib
 * @run main/othervm -Xmx1g ThreadCountLimit
 */

import java.util.concurrent.CountDownLatch;
import java.util.ArrayList;

import jdk.test.lib.Platform;
import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;

public class ThreadCountLimit {

  static final int TIME_LIMIT_MS = 5000; // Create as many threads as possible in 5 sec

  static class Worker extends Thread {
    private final CountDownLatch startSignal;

    Worker(CountDownLatch startSignal) {
      this.startSignal = startSignal;
    }

    @Override
    public void run() {
      try {
        startSignal.await();
      } catch (InterruptedException e) {
        throw new Error("Unexpected", e);
      }
    }
  }

  // Limit the number of threads that we're allowed to create
  private static String ULIMIT_CMD = "ulimit -u 10000";

  public static void main(String[] args) throws Exception {
    if (args.length == 0) {
      // Called from the driver process. Forks the test process.

      if (Platform.isLinux()) {
        // The test command
        String JAVA_CMD = ProcessTools.getCommandLine(
            ProcessTools.createTestJavaProcessBuilder(ThreadCountLimit.class.getName()));

        // On Linux this test sometimes hit the limit for the maximum number of memory mappings,
        // which leads to various other failure modes. Run this test with a limit on how many
        // threads that the process is allowed to create.

        // Call back into this function, with args.length > 0, and the with the ulimit set
        ProcessTools.executeCommand("bash", "-c", ULIMIT_CMD + " && " + JAVA_CMD + " dummy")
            .shouldHaveExitValue(0);
      } else {
        // Other platforms don't seem to get the same problems,
        // so just run the test without any extra limits set.
        test();
      }
    } else {
      // Run the actual test code
      test();
    }
  }

  public static void test() {
    CountDownLatch startSignal = new CountDownLatch(1);
    ArrayList<Worker> workers = new ArrayList<Worker>();

    boolean reachedTimeLimit = false;
    boolean reachedNativeOOM = false;
    int countAtTimeLimit = -1;
    int countAtNativeOOM = -1;

    // This is dangerous loop: it depletes system resources,
    // so doing additional things there that may end up allocating
    // Java/native memory risks failing the VM prematurely.
    // Avoid doing unnecessary calls, printouts, etc.

    int count = 1;
    long start = System.currentTimeMillis();
    try {
      while (true) {
        Worker w = new Worker(startSignal);
        w.start();
        workers.add(w);
        count++;

        long end = System.currentTimeMillis();
        if ((end - start) > TIME_LIMIT_MS) {
          reachedTimeLimit = true;
          countAtTimeLimit = count;
          break;
        }
      }
    } catch (OutOfMemoryError e) {
      if (e.getMessage().contains("unable to create native thread")) {
        // Linux, macOS path
        reachedNativeOOM = true;
        countAtNativeOOM = count;
      } else {
        throw e;
      }
    }

    startSignal.countDown();

    try {
      for (Worker w : workers) {
        w.join();
      }
    } catch (InterruptedException e) {
      throw new Error("Unexpected", e);
    }

    // Now that all threads have joined, we are away from dangerous
    // VM state and have enough memory to perform any other things.
    if (reachedTimeLimit) {
       // Windows path or a system with very large ulimit
       System.out.println("INFO: reached the time limit " + TIME_LIMIT_MS +
                          " ms, with " + countAtTimeLimit + " threads created");
    } else if (reachedNativeOOM) {
       System.out.println("INFO: reached this process thread count limit with " +
                           countAtNativeOOM + " threads created");
    } else {
      System.out.println("INFO: Completed with " + countAtTimeLimit + " threads created");
    }
  }
}
