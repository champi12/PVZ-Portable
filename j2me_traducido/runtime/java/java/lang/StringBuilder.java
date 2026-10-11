package java.lang;

public final class StringBuilder {
    private char[] value;
    private int count;

    public StringBuilder() { this(16); }
    public StringBuilder(int cap) { value = new char[cap < 0 ? 0 : cap]; }
    public StringBuilder(String s) { this(s.length() + 16); append(s); }

    char[] getValue() { return value; }
    public int length() { return count; }
    public int capacity() { return value.length; }
    public void ensureCapacity(int min) {
        if (min > value.length) {
            int n = value.length * 2 + 2;
            if (n < min) n = min;
            char[] v = new char[n];
            System.arraycopy(value, 0, v, 0, count);
            value = v;
        }
    }
    public void setLength(int len) {
        if (len < 0) throw new StringIndexOutOfBoundsException();
        ensureCapacity(len);
        for (int i = count; i < len; i++) value[i] = 0;
        count = len;
    }
    public char charAt(int i) {
        if (i < 0 || i >= count) throw new StringIndexOutOfBoundsException();
        return value[i];
    }
    public void setCharAt(int i, char c) {
        if (i < 0 || i >= count) throw new StringIndexOutOfBoundsException();
        value[i] = c;
    }
    public StringBuilder append(String s) {
        if (s == null) s = "null";
        int n = s.length();
        ensureCapacity(count + n);
        s.getChars(0, n, value, count);
        count += n;
        return this;
    }
    public StringBuilder append(Object o) { return append(String.valueOf(o)); }
    public StringBuilder append(char c) {
        ensureCapacity(count + 1);
        value[count++] = c;
        return this;
    }
    public StringBuilder append(char[] c) { return append(new String(c)); }
    public StringBuilder append(int i) { return append(Integer.toString(i)); }
    public StringBuilder append(long l) { return append(Long.toString(l)); }
    public StringBuilder append(boolean b) { return append(b ? "true" : "false"); }
    public StringBuilder insert(int off, String s) {
        if (off < 0 || off > count) throw new StringIndexOutOfBoundsException();
        if (s == null) s = "null";
        int n = s.length();
        ensureCapacity(count + n);
        System.arraycopy(value, off, value, off + n, count - off);
        s.getChars(0, n, value, off);
        count += n;
        return this;
    }
    public StringBuilder insert(int off, Object o) { return insert(off, String.valueOf(o)); }
    public StringBuilder insert(int off, char c) { return insert(off, String.valueOf(c)); }
    public StringBuilder insert(int off, int i) { return insert(off, Integer.toString(i)); }
    public StringBuilder delete(int start, int end) {
        if (end > count) end = count;
        if (start < 0 || start > end) throw new StringIndexOutOfBoundsException();
        int n = end - start;
        if (n > 0) {
            System.arraycopy(value, end, value, start, count - end);
            count -= n;
        }
        return this;
    }
    public StringBuilder deleteCharAt(int i) { return delete(i, i + 1); }
    public StringBuilder reverse() {
        for (int i = 0, j = count - 1; i < j; i++, j--) { char t = value[i]; value[i] = value[j]; value[j] = t; }
        return this;
    }
    public String toString() { return new String(value, 0, count); }
}
