/*
 * Copyright (c) 2016, 2023, Oracle and/or its affiliates. All rights reserved.
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

/*
 * @test
 * @bug 8162415
 * @summary Test warnings for ignored properties.
 * @requires vm.flagless
 * @modules java.base/jdk.internal.misc
 * @library /test/lib
 * @run driver ModuleOptionsWarn
 */

import java.util.Map;
import jdk.test.lib.process.OutputAnalyzer;
import jdk.test.lib.process.ProcessTools;

// Test that the VM behaves correctly when processing command line module system properties.
public class ModuleOptionsWarn {

    public static void main(String[] args) throws Exception {

        // Test that a warning is not issued for extraneous jdk.module properties.
        OutputAnalyzer output = ProcessTools.executeLimitedTestJava(
            "-XX:+PrintWarnings", "-Djdk.module.ignored", "-version");
        output.shouldNotContain("Ignoring system property option");
        output.shouldHaveExitValue(0);

        // Test that a warning is issued for a reserved jdk.module property.
        output = ProcessTools.executeLimitedTestJava(
            "-XX:+PrintWarnings", "-Djdk.module.addmods", "-version");
        output.shouldContain("Ignoring system property option");
        output.shouldHaveExitValue(0);

        // Test that a warning is issued for a reserved jdk.module property ending in '.'.
        output = ProcessTools.executeLimitedTestJava(
            "-XX:+PrintWarnings", "-Djdk.module.limitmods.", "-version");
        output.shouldContain("Ignoring system property option");
        output.shouldHaveExitValue(0);

        // Test that a warning is issued for a reserved jdk.module property ending in '='.
        output = ProcessTools.executeLimitedTestJava(
            "-XX:+PrintWarnings", "-Djdk.module.addexports=", "-version");
        output.shouldContain("Ignoring system property option");
        output.shouldHaveExitValue(0);

        // Test that a warning is issued for a reserved jdk.module property ending in ".stuff"
        output = ProcessTools.executeLimitedTestJava(
            "-XX:+PrintWarnings", "-Djdk.module.addreads.stuff", "-version");
        output.shouldContain("Ignoring system property option");
        output.shouldHaveExitValue(0);

        // Test that a warning is issued for a reserved jdk.module property ending in "=stuff"
        output = ProcessTools.executeLimitedTestJava(
            "-XX:+PrintWarnings", "-Djdk.module.path=stuff", "-version");
        output.shouldContain("Ignoring system property option");
        output.shouldHaveExitValue(0);

        // Test that a warning is issued for a reserved jdk.module property ending in ".="
        output = ProcessTools.executeLimitedTestJava(
            "-XX:+PrintWarnings", "-Djdk.module.upgrade.path.=xx", "-version");
        output.shouldContain("Ignoring system property option");
        output.shouldHaveExitValue(0);

        // Test that a warning is issued for a reserved jdk.module property ending in ".<num>"
        output = ProcessTools.executeLimitedTestJava(
            "-XX:+PrintWarnings", "-Djdk.module.patch.3=xx", "-version");
        output.shouldContain("Ignoring system property option");
        output.shouldHaveExitValue(0);

        // Test that a warning can be suppressed for module related properties that get ignored.
        output = ProcessTools.executeLimitedTestJava(
            "-Djdk.module.addmods", "-XX:-PrintWarnings", "-version");
        output.shouldNotContain("Ignoring system property option");
        output.shouldHaveExitValue(0);

        // Test that a warning is not issued for properties of the form "jdk.module.main"
        output = ProcessTools.executeLimitedTestJava(
            "-XX:+PrintWarnings", "-Djdk.module.main.ignored", "-version");
        output.shouldNotContain("Ignoring system property option");
        output.shouldHaveExitValue(0);

        // Test that a warning is issued for module related properties specified using _JAVA_OPTIONS.
        ProcessBuilder pb = ProcessTools.createLimitedTestJavaProcessBuilder("-XX:+PrintWarnings", "-version");
        Map<String, String> env = pb.environment();
        env.put("_JAVA_OPTIONS", "-Djdk.module.addreads");
        output = ProcessTools.executeProcess(pb);
        output.shouldContain("Ignoring system property option");
        output.shouldHaveExitValue(0);
    }
}
