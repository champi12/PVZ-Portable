package java.lang;

public final class String {
    char[] value;
    int offset;
    int count;
    private int hash;

    public String() { value = new char[0]; }
    public String(char[] v) { this(v, 0, v.length); }
    public String(char[] v, int off, int len) {
        if (off < 0 || len < 0 || off + len > v.length) throw new StringIndexOutOfBoundsException();
        value = new char[len];
        System.arraycopy(v, off, value, 0, len);
        count = len;
    }
    String(int off, int len, char[] v) { value = v; offset = off; count = len; }
    public String(String s) { value = s.value; offset = s.offset; count = s.count; }
    public String(StringBuffer sb) { this(sb.getValue(), 0, sb.length()); }
    public String(StringBuilder sb) { this(sb.getValue(), 0, sb.length()); }
    public String(byte[] b) { this(b, 0, b.length); }
    public String(byte[] b, int off, int len) {
        value = new char[len];
        for (int i = 0; i < len; i++) value[i] = (char)(b[off + i] & 0xFF);
        count = len;
    }
    public String(byte[] b, String enc) { this(b, 0, b.length, enc); }
    public String(byte[] b, int off, int len, String enc) {
        if (enc != null && (enc.equals("UTF-8") || enc.equals("UTF8") || enc.equals("utf-8"))) {
            char[] out = new char[len];
            int n = 0, i = off, end = off + len;
            while (i < end) {
                int c = b[i++] & 0xFF;
                if (c < 0x80) out[n++] = (char)c;
                else if ((c & 0xE0) == 0xC0 && i < end) out[n++] = (char)(((c & 0x1F) << 6) | (b[i++] & 0x3F));
                else if ((c & 0xF0) == 0xE0 && i + 1 < end) { out[n++] = (char)(((c & 0x0F) << 12) | ((b[i] & 0x3F) << 6) | (b[i + 1] & 0x3F)); i += 2; }
                else out[n++] = (char)c;
            }
            value = out; count = n;
        } else {
            value = new char[len];
            for (int i = 0; i < len; i++) value[i] = (char)(b[off + i] & 0xFF);
            count = len;
        }
    }

    public int length() { return count; }
    public char charAt(int i) {
        if (i < 0 || i >= count) throw new StringIndexOutOfBoundsException();
        return value[offset + i];
    }
    public void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin) {
        System.arraycopy(value, offset + srcBegin, dst, dstBegin, srcEnd - srcBegin);
    }
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof String)) return false;
        String s = (String)o;
        if (s.count != count) return false;
        for (int i = 0; i < count; i++) if (value[offset + i] != s.value[s.offset + i]) return false;
        return true;
    }
    public int hashCode() {
        int h = hash;
        if (h == 0) {
            for (int i = 0; i < count; i++) h = 31 * h + value[offset + i];
            hash = h;
        }
        return h;
    }
    public int compareTo(String s) {
        int n = count < s.count ? count : s.count;
        for (int i = 0; i < n; i++) {
            int d = value[offset + i] - s.value[s.offset + i];
            if (d != 0) return d;
        }
        return count - s.count;
    }
    public int indexOf(int ch) { return indexOf(ch, 0); }
    public int indexOf(int ch, int from) {
        if (from < 0) from = 0;
        for (int i = from; i < count; i++) if (value[offset + i] == ch) return i;
        return -1;
    }
    public int indexOf(String s) { return indexOf(s, 0); }
    public int indexOf(String s, int from) {
        if (from < 0) from = 0;
        int n = s.count;
        outer:
        for (int i = from; i + n <= count; i++) {
            for (int j = 0; j < n; j++) if (value[offset + i + j] != s.value[s.offset + j]) continue outer;
            return i;
        }
        return -1;
    }
    public int lastIndexOf(int ch) {
        for (int i = count - 1; i >= 0; i--) if (value[offset + i] == ch) return i;
        return -1;
    }
    public boolean startsWith(String s) { return s.count <= count && indexOf(s, 0) == 0; }
    public boolean endsWith(String s) {
        if (s.count > count) return false;
        for (int j = 0; j < s.count; j++) if (value[offset + count - s.count + j] != s.value[s.offset + j]) return false;
        return true;
    }
    public String substring(int begin) { return substring(begin, count); }
    public String substring(int begin, int end) {
        if (begin < 0 || end > count || begin > end) throw new StringIndexOutOfBoundsException();
        if (begin == 0 && end == count) return this;
        return new String(offset + begin, end - begin, value);
    }
    public String concat(String s) {
        if (s.count == 0) return this;
        char[] v = new char[count + s.count];
        getChars(0, count, v, 0);
        s.getChars(0, s.count, v, count);
        return new String(0, v.length, v);
    }
    public String replace(char a, char b) {
        char[] v = toCharArray();
        for (int i = 0; i < v.length; i++) if (v[i] == a) v[i] = b;
        return new String(0, v.length, v);
    }
    public char[] toCharArray() {
        char[] v = new char[count];
        getChars(0, count, v, 0);
        return v;
    }
    public byte[] getBytes() {
        byte[] b = new byte[count];
        for (int i = 0; i < count; i++) b[i] = (byte)value[offset + i];
        return b;
    }
    public byte[] getBytes(String enc) {
        if (enc != null && (enc.equals("UTF-8") || enc.equals("UTF8"))) {
            StringBuffer sb = new StringBuffer();
            int n = 0;
            for (int i = 0; i < count; i++) {
                char c = value[offset + i];
                n += c < 0x80 ? 1 : (c < 0x800 ? 2 : 3);
            }
            byte[] b = new byte[n];
            n = 0;
            for (int i = 0; i < count; i++) {
                char c = value[offset + i];
                if (c < 0x80) b[n++] = (byte)c;
                else if (c < 0x800) { b[n++] = (byte)(0xC0 | (c >> 6)); b[n++] = (byte)(0x80 | (c & 0x3F)); }
                else { b[n++] = (byte)(0xE0 | (c >> 12)); b[n++] = (byte)(0x80 | ((c >> 6) & 0x3F)); b[n++] = (byte)(0x80 | (c & 0x3F)); }
            }
            return b;
        }
        return getBytes();
    }
    public String toLowerCase() {
        char[] v = toCharArray();
        for (int i = 0; i < v.length; i++) v[i] = Character.toLowerCase(v[i]);
        return new String(0, v.length, v);
    }
    public String toUpperCase() {
        char[] v = toCharArray();
        for (int i = 0; i < v.length; i++) v[i] = Character.toUpperCase(v[i]);
        return new String(0, v.length, v);
    }
    public String trim() {
        int b = 0, e = count;
        while (b < e && value[offset + b] <= ' ') b++;
        while (e > b && value[offset + e - 1] <= ' ') e--;
        return substring(b, e);
    }
    public String toString() { return this; }
    public static String valueOf(Object o) { return o == null ? "null" : o.toString(); }
    public static String valueOf(int i) { return Integer.toString(i); }
    public static String valueOf(long l) { return Long.toString(l); }
    public static String valueOf(char c) { char[] v = new char[1]; v[0] = c; return new String(0, 1, v); }
    public static String valueOf(boolean b) { return b ? "true" : "false"; }
    public static String valueOf(char[] v) { return new String(v); }
}
