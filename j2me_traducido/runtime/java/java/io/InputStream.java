package java.io;

public abstract class InputStream {
    public InputStream() {}
    public abstract int read() throws IOException;
    public int read(byte[] b) throws IOException { return read(b, 0, b.length); }
    public int read(byte[] b, int off, int len) throws IOException {
        if (len == 0) return 0;
        int c = read();
        if (c < 0) return -1;
        b[off] = (byte)c;
        int i = 1;
        for (; i < len; i++) {
            c = read();
            if (c < 0) break;
            b[off + i] = (byte)c;
        }
        return i;
    }
    public long skip(long n) throws IOException {
        long i = 0;
        while (i < n && read() >= 0) i++;
        return i;
    }
    public int available() throws IOException { return 0; }
    public void close() throws IOException {}
    public void mark(int limit) {}
    public void reset() throws IOException { throw new IOException(); }
    public boolean markSupported() { return false; }
}
