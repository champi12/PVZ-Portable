package javax.microedition.lcdui;

import java.io.InputStream;
import java.io.IOException;

public class Image {
    int[] pixels;   // ARGB, row-major
    int width;
    int height;
    boolean mutable;

    private Image(int w, int h, int[] p, boolean m) { width = w; height = h; pixels = p; mutable = m; }

    public static Image createImage(int w, int h) {
        if (w <= 0 || h <= 0) throw new IllegalArgumentException();
        int[] p = new int[w * h];
        for (int i = 0; i < p.length; i++) p[i] = 0xFFFFFFFF;
        return new Image(w, h, p, true);
    }
    public static Image createImage(Image src) {
        if (!src.mutable) return src;
        int[] p = new int[src.pixels.length];
        System.arraycopy(src.pixels, 0, p, 0, p.length);
        return new Image(src.width, src.height, p, false);
    }
    public static Image createImage(String name) throws IOException {
        InputStream in = name.getClass().getResourceAsStream(name);
        if (in == null) throw new IOException(name);
        byte[] buf = new byte[in.available()];
        int n = 0;
        while (n < buf.length) {
            int c = in.read(buf, n, buf.length - n);
            if (c < 0) break;
            n += c;
        }
        return createImage(buf, 0, n);
    }
    public static Image createImage(byte[] data, int off, int len) {
        int[] wh = new int[2];
        int[] p = decode(data, off, len, wh);
        if (p == null) throw new IllegalArgumentException();
        return new Image(wh[0], wh[1], p, false);
    }
    public static Image createImage(InputStream in) throws IOException {
        byte[] buf = new byte[4096];
        int n = 0;
        for (;;) {
            if (n == buf.length) {
                byte[] b = new byte[n * 2];
                System.arraycopy(buf, 0, b, 0, n);
                buf = b;
            }
            int c = in.read(buf, n, buf.length - n);
            if (c < 0) break;
            n += c;
        }
        return createImage(buf, 0, n);
    }
    public static Image createRGBImage(int[] rgb, int w, int h, boolean alpha) {
        if (w <= 0 || h <= 0) throw new IllegalArgumentException();
        int[] p = new int[w * h];
        System.arraycopy(rgb, 0, p, 0, w * h);
        if (!alpha) for (int i = 0; i < p.length; i++) p[i] |= 0xFF000000;
        swapToNative(p, 0, p.length);
        return new Image(w, h, p, false);
    }
    public static Image createImage(Image src, int x, int y, int w, int h, int transform) {
        Image img = new Image(1, 1, new int[1], true);
        int[] out = new int[w * h];
        int[] wh = new int[2];
        Graphics.transformRegion(src.pixels, src.width, x, y, w, h, transform, out, wh);
        img.pixels = out;
        img.width = wh[0];
        img.height = wh[1];
        img.mutable = false;
        return img;
    }

    /** Swaps red and blue on platforms whose native pixel layout is ABGR (no-op elsewhere). */
    private static native void swapToNative(int[] p, int off, int len);
    private static native int[] decode(byte[] data, int off, int len, int[] wh);

    public Graphics getGraphics() {
        if (!mutable) throw new IllegalStateException();
        return new Graphics(this);
    }
    public int getWidth() { return width; }
    public int getHeight() { return height; }
    public boolean isMutable() { return mutable; }
    public void getRGB(int[] rgb, int offset, int scanlength, int x, int y, int w, int h) {
        if (x < 0 || y < 0 || x + w > width || y + h > height) throw new IllegalArgumentException();
        for (int j = 0; j < h; j++) {
            System.arraycopy(pixels, (y + j) * width + x, rgb, offset + j * scanlength, w);
            swapToNative(rgb, offset + j * scanlength, w);
        }
    }
}
