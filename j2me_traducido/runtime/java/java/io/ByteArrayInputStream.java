package java.io;

public class ByteArrayInputStream extends InputStream {
    protected byte[] buf;
    protected int pos;
    protected int count;
    protected int mark;

    public ByteArrayInputStream(byte[] b) { buf = b; count = b.length; }
    public ByteArrayInputStream(byte[] b, int off, int len) {
        buf = b; pos = off; mark = off;
        count = off + len > b.length ? b.length : off + len;
    }
    public int read() { return pos < count ? (buf[pos++] & 0xFF) : -1; }
    public int read(byte[] b, int off, int len) {
        if (pos >= count) return -1;
        if (len > count - pos) len = count - pos;
        System.arraycopy(buf, pos, b, off, len);
        pos += len;
        return len;
    }
    public long skip(long n) {
        if (n > count - pos) n = count - pos;
        if (n < 0) return 0;
        pos += (int)n;
        return n;
    }
    public int available() { return count - pos; }
    public boolean markSupported() { return true; }
    public void mark(int limit) { mark = pos; }
    public void reset() { pos = mark; }
    public void close() {}
}
