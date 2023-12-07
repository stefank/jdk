/*
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
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

package jdk.test.lib.process;

import java.io.IOException;
import java.io.PrintStream;
import java.nio.charset.Charset;
import java.time.Instant;
import java.util.concurrent.TimeUnit;

public class ProcessExecutor {
    public static class ProcessExecutionException extends RuntimeException {
        private static final long serialVersionUID = 1L;

        public ProcessExecutionException(Throwable cause) {
            super(cause);
        }
    }

    /* The executed process */
    private final Process process;
    /* The task to pump stdout */
    private final StreamTask outTask;
    /* The task to pump stderr */
    private final StreamTask errTask;
    /* The process' exit value, or null if the process
     * has not been joined by waitFor */
    private Integer exitValue;

    /**
     * Creates a ProcessExecutor which will help starting and drinving a
     * process to its completion. The process is started in this constructor.
     *
     * @param pb The process builder describing the process to be started.
     * @param input The input to send to the spawned process, or null.
     * @param cs The charset, or null
     */
    public ProcessExecutor(ProcessBuilder pb, String input, Charset cs) throws IOException {
        // Start the process
        this.process = ProcessTools.privilegedStart(pb);

        // Log that we are about to gather the stdout and stderr
        logProgress("Gathering output");

        outTask = new StreamTask(process.getInputStream(), cs);
        errTask = new StreamTask(process.getErrorStream(), cs);

        // Send input to the process
        if (input != null) {
            try (PrintStream ps = new PrintStream(process.getOutputStream())) {
                ps.print(input);
            }
        }
    }

    public ProcessExecutor(ProcessBuilder pb, String input) throws IOException {
        this(pb, input, null /* cs */);
    }

    public ProcessExecutor(ProcessBuilder pb) throws IOException {
        this(pb, null /* input */);
    }

    /**
     * @return true iff waitFor has been called and the process has been found
     *         to have exited.
     */
    public boolean hasFinished() {
        return exitValue != null;
    }

    /**
     * Throws an exception if the process has not "finished".
     */
    private void checkHasFinished() {
        if (!hasFinished()) {
            throw new RuntimeException("The process has not finished");
        }
    }

    /**
     * @return The stdout output of the finished process
     */
    public String getStdout() {
        checkHasFinished();
        return outTask.get();
    }

    /**
     * @return The stderr output of the finished process
     */
    public String getStderr() {
        checkHasFinished();
        return errTask.get();
    }

    /**
     * @return The exit value of the finished process
     */
    public int getExitValue() {
        checkHasFinished();
        return exitValue;
    }

    /**
     * @return An OutputAnalyzer instance of the finished process
     */
    public OutputAnalyzer getOutputAnalyzer() {
        checkHasFinished();
        return new OutputAnalyzer(getStdout(), getStderr(), getExitValue());
    }

    /**
     * Log some info about the progress that has been made.
     * This is useful to figure out hangs in the testing.
     */
    private final void logProgress(String state) {
        System.out.println("[" + Instant.now().toString() + "] " + state
                           + " for process " + process.pid());
        System.out.flush();
    }

    /**
     * Delegates to Process::destroy
     */
    public void destroy() {
        process.destroy();
    }

    /**
     * Delegates to Process::destroyForcibly
     */
    public void destroyForcibly() {
        process.destroyForcibly();
    }

    /**
     * Delegates to Process::isAlive
     */
    public boolean isAlive() {
        return process.isAlive();
    }

    /**
     * Delegates to Process::pid
     */
    public long pid() {
        return process.pid();
    }

    /**
     * Wait for the process to finish and record the exitValue
     *
     * @return true iff the process exited, false if waitfor timed out
     */
    private boolean waitForInner(long timeout, TimeUnit unit) throws InterruptedException {
        if (timeout == 0L) {
            exitValue = process.waitFor();
            return true;
        }

        boolean exited = process.waitFor(timeout, unit);
        if (exited) {
            exitValue = process.exitValue();
            return true;
        }

        return false;
    }

    /**
     * Wait for the process to finish and record the exitValue
     *
     * @return true iff the process exited, false if waitfor timed out
     */
    public boolean waitFor(long timeout, TimeUnit unit) throws InterruptedException {
        if (exitValue != null) {
            // We've already waited for this process.
            return true;
        }

        logProgress("Waiting for completion");

        boolean aborted = true;
        try {
            System.out.println("Waiting for: " + timeout);
            boolean exited = waitForInner(timeout, unit);
            if (exited) {
                logProgress("Waiting for completion finished");
            }

            aborted = false;
            return exited;
        } finally {
            if (aborted) {
                logProgress("Waiting for completion FAILED");
            }
        }
    }

    /**
     * Wait for the process to finish and record the exitValue
     */
    public void waitForThrowing() throws InterruptedException {
        waitFor(0L, TimeUnit.SECONDS);
    }

    /**
     * Wait for the process to finish and record the exitValue.
     *
     * Swallows the InterruptedException.
     *
     * @return true iff the process exited, false if waitfor timed out
     */
    public void waitFor() {
        try {
            waitForThrowing();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new ProcessExecutionException(e);
        }
    }

    /**
     * Wait for the process to exit and return the stdout output.
     */
    public String waitForStdout() {
        waitFor();
        return getStdout();
    }

    /**
     * Wait for the process to exit and return the stderr output.
     */
    public String waitForStderr() {
        waitFor();
        return getStderr();
    }

    /**
     * Wait for the process to exit and return the exit value.
     */
    public int waitForExitValue() {
        waitFor();
        return exitValue;
    }

    /**
     * Wait for the process to exit and return an OutputAnalyzer instance.
     */
    public OutputAnalyzer waitForOutputAnalyzer() {
        waitFor();
        return getOutputAnalyzer();
    }
}
