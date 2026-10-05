package java.io;

public class PrintStream {
    public PrintStream() {}
    private static native void write(String s, boolean newline);
    public void println(String s) { write(String.valueOf(s), true); }
    public void println(Object o) { write(String.valueOf(o), true); }
    public void println(int i) { write(String.valueOf(i), true); }
    public void println() { write("", true); }
    public void print(String s) { write(String.valueOf(s), false); }
    public void print(int i) { write(String.valueOf(i), false); }
}
