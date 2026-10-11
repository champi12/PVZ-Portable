package java.util;

public class Random {
    private long seed;
    public Random() { this(System.currentTimeMillis()); }
    public Random(long s) { setSeed(s); }
    public void setSeed(long s) { seed = (s ^ 0x5DEECE66DL) & ((1L << 48) - 1); }
    protected int next(int bits) {
        seed = (seed * 0x5DEECE66DL + 0xBL) & ((1L << 48) - 1);
        return (int)(seed >>> (48 - bits));
    }
    public int nextInt() { return next(32); }
    public int nextInt(int n) {
        if (n <= 0) throw new IllegalArgumentException();
        if ((n & -n) == n) return (int)((n * (long)next(31)) >> 31);
        int bits, val;
        do {
            bits = next(31);
            val = bits % n;
        } while (bits - val + (n - 1) < 0);
        return val;
    }
    public long nextLong() { return ((long)next(32) << 32) + next(32); }
}
