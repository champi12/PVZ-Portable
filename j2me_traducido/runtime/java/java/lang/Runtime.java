package java.lang;

public class Runtime {
    private static Runtime instance;
    private Runtime() {}
    public static Runtime getRuntime() {
        if (instance == null) instance = new Runtime();
        return instance;
    }
    public void gc() { System.gc(); }
    public native long freeMemory();
    public native long totalMemory();
    public native void exit(int code);
}
