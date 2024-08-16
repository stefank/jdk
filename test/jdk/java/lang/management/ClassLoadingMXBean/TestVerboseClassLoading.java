/*
 * @test
 * @bug     8338139
 * @summary Basic unit test of ClassLoadingMXBean.set/isVerbose() when
 *          related unified logging is enabled.
 *
 * @run main/othervm -Xlog:class+load=trace:file=vm.log TestVerboseClassLoading false
 * @run main/othervm -Xlog:class+load=debug:file=vm.log TestVerboseClassLoading false
 * @run main/othervm -Xlog:class+load=info:file=vm.log TestVerboseClassLoading false
 * 
 * @run main/othervm -Xlog:class+load=off TestVerboseClassLoading false
 * @run main/othervm -Xlog:class+load=error TestVerboseClassLoading false
 * @run main/othervm -Xlog:class+load=warning TestVerboseClassLoading false
 * 
 * @run main/othervm -Xlog:class+load=info TestVerboseClassLoading true
 * @run main/othervm -Xlog:class+load=trace TestVerboseClassLoading true
 * @run main/othervm -Xlog:class+load=debug TestVerboseClassLoading true
 * 
 * @run main/othervm -Xlog:class+load*=info TestVerboseClassLoading true
 *
 * @run main/othervm -Xlog:class+load=info,class+load+cause=off TestVerboseClassLoading true
 * @run main/othervm -Xlog:class+load=off,class+load+cause=info TestVerboseClassLoading true
 */

import java.lang.management.*;

public class TestVerboseClassLoading {

    public static void main(String[] args) throws Exception {
        ClassLoadingMXBean mxBean = ManagementFactory.getClassLoadingMXBean();
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
