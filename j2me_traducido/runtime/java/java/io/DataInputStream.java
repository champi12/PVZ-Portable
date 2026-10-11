package java.io;

public class DataInputStream extends InputStream {
    protected InputStream in;

    public DataInputStream(InputStream in) { this.in = in; }
    public int read() throws IOException { return in.read(); }
    public int read(byte[] b) throws IOException { return in.read(b, 0, b.length); }
    public int read(byte[] b, int off, int len) throws IOException { return in.read(b, off, len); }
    public long skip(long n) throws IOException { return in.skip(n); }
    public int available() throws IOException { return in.available(); }
    public void close() throws IOException { in.close(); }
    public void mark(int limit) { in.mark(limit); }
    public void reset() throws IOException { in.reset(); }
    public boolean markSupported() { return in.markSupported(); }

    public final void readFully(byte[] b) throws IOException { readFully(b, 0, b.length); }
    public final void readFully(byte[] b, int off, int len) throws IOException {
        int n = 0;
        while (n < len) {
            int c = in.read(b, off + n, len - n);
            if (c < 0) throw new EOFException();
            n += c;
        }
    }
    public final int skipBytes(int n) throws IOException {
        int t = 0;
        while (t < n) {
            int c = (int)in.skip(n - t);
            if (c <= 0) break;
            t += c;
        }
        return t;
    }
    private int r() throws IOException {
        int c = in.read();
        if (c < 0) throw new EOFException();
        return c;
    }
    public final boolean readBoolean() throws IOException { return r() != 0; }
    public final byte readByte() throws IOException { return (byte)r(); }
    public final int readUnsignedByte() throws IOException { return r(); }
    public final short readShort() throws IOException { int a = r(); return (short)((a << 8) | r()); }
    public final int readUnsignedShort() throws IOException { int a = r(); return (a << 8) | r(); }
    public final char readChar() throws IOException { int a = r(); return (char)((a << 8) | r()); }
    public final int readInt() throws IOException {
        int a = r(), b = r(), c = r();
        return (a << 24) | (b << 16) | (c << 8) | r();
    }
    public final long readLong() throws IOException {
        long hi = readInt();
        return (hi << 32) | (readInt() & 0xFFFFFFFFL);
    }
    public final String readUTF() throws IOException {
        int len = readUnsignedShort();
        byte[] b = new byte[len];
        readFully(b);
        char[] out = new char[len];
        int n = 0, i = 0;
        while (i < len) {
            int c = b[i++] & 0xFF;
            if (c < 0x80) out[n++] = (char)c;
            else if ((c & 0xE0) == 0xC0) {
                if (i >= len) throw new UTFDataFormatException();
                out[n++] = (char)(((c & 0x1F) << 6) | (b[i++] & 0x3F));
            } else if ((c & 0xF0) == 0xE0) {
                if (i + 1 >= len) throw new UTFDataFormatException();
                out[n++] = (char)(((c & 0x0F) << 12) | ((b[i] & 0x3F) << 6) | (b[i + 1] & 0x3F));
                i += 2;
            } else throw new UTFDataFormatException();
        }
        return new String(out, 0, n);
    }
}
