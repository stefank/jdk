/*
 * @test
 * @bug     8338139
 * @summary Basic unit test of TestVerboseMemory.set/isVerbose() when
 *          related unified logging is enabled.
 *
 * @run main/othervm -Xlog:gc=trace:file=vm.log TestVerboseMemory false
 * @run main/othervm -Xlog:gc=debug:file=vm.log TestVerboseMemory false
 * @run main/othervm -Xlog:gc=info:file=vm.log TestVerboseMemory false
 * 
 * @run main/othervm -Xlog:gc=off TestVerboseMemory false
 * @run main/othervm -Xlog:gc=error TestVerboseMemory false
 * @run main/othervm -Xlog:gc=warning TestVerboseMemory false
 * 
 * @run main/othervm -Xlog:gc=info TestVerboseMemory true
 * @run main/othervm -Xlog:gc=trace TestVerboseMemory true
 * @run main/othervm -Xlog:gc=debug TestVerboseMemory true
 * 
 * @run main/othervm -Xlog:gc*=info TestVerboseMemory true
 * @run main/othervm -Xlog:gc*=debug TestVerboseMemory true
 * @run main/othervm -Xlog:gc*=trace TestVerboseMemory true
 *
 * @run main/othervm -Xlog:gc=info,gc+init=off TestVerboseMemory true
 * @run main/othervm -Xlog:gc=off,gc+init=info TestVerboseMemory false
 */

import java.lang.management.*;

public class TestVerboseMemory {

    public static void main(String[] args) throws Exception {
        MemoryMXBean mxBean = ManagementFactory.getMemoryMXBean();
        boolean expected = Boolean.parseBoolean(args[0]);
        boolean initial = mxBean.isVerbose();
        if (expected != initial) {
            throw new Error("Initial verbosity setting was unexpectedly " + initial);
        }
        mxBean.setVerbose(false);
        if (mxBean.isVerbose()) {
            throw new Error("Verbosity was still enabled");
        }
        mxBean.setVerbose(true);
        if (!mxBean.isVerbose()) {
            throw new Error("Verbosity was still disabled");
        }
        // Turn off again as a double-check and also to avoid excessive logging
        mxBean.setVerbose(false);
        if (mxBean.isVerbose()) {
            throw new Error("Verbosity was still enabled");
        }
    }
}
