package java.lang;

public final class Integer {
    private Integer() {}
    public static String toString(int i) { return toString(i, 10); }
    public static String toString(int i, int radix) { return Long.toString(i, radix); }
    public static String toHexString(int i) { return Long.toString(i & 0xFFFFFFFFL, 16); }
    public static int parseInt(String s) { return parseInt(s, 10); }
    public static int parseInt(String s, int radix) {
        long v = Long.parseLong(s, radix);
        if (v < -2147483648L || v > 2147483647L) throw new NumberFormatException(s);
        return (int)v;
    }
}
