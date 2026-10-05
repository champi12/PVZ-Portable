package java.lang;

public final class Long {
    private Long() {}
    public static String toString(long i) { return toString(i, 10); }
    public static String toString(long i, int radix) {
        if (i == 0) return "0";
        char[] buf = new char[65];
        int pos = 65;
        boolean neg = i < 0;
        if (!neg) i = -i;
        while (i != 0) {
            int d = (int)-(i % radix);
            buf[--pos] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
            i /= radix;
        }
        if (neg) buf[--pos] = '-';
        return new String(buf, pos, 65 - pos);
    }
    public static long parseLong(String s) { return parseLong(s, 10); }
    public static long parseLong(String s, int radix) {
        if (s == null || s.length() == 0) throw new NumberFormatException();
        int i = 0;
        boolean neg = false;
        char c = s.charAt(0);
        if (c == '-' || c == '+') { neg = c == '-'; i = 1; if (s.length() == 1) throw new NumberFormatException(s); }
        long v = 0;
        for (; i < s.length(); i++) {
            c = s.charAt(i);
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
            else throw new NumberFormatException(s);
            if (d >= radix) throw new NumberFormatException(s);
            v = v * radix + d;
        }
        return neg ? -v : v;
    }
}
