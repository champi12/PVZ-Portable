package java.lang;

import java.io.PrintStream;

public final class System {
    private System() {}
    public static final PrintStream out = new PrintStream();
    public static final PrintStream err = out;
    public static native void arraycopy(Object src, int srcPos, Object dst, int dstPos, int length);
    public static native long currentTimeMillis();
    public static native void gc();
    public static String getProperty(String key) {
        if ("microedition.platform".equals(key)) return "PSP";
        if ("microedition.encoding".equals(key)) return "ISO-8859-1";
        if ("microedition.locale".equals(key)) return "en-US";
        if ("microedition.configuration".equals(key)) return "CLDC-1.0";
        if ("microedition.profiles".equals(key)) return "MIDP-2.0";
        return null;
    }
    public static void exit(int code) { Runtime.getRuntime().exit(code); }
}
