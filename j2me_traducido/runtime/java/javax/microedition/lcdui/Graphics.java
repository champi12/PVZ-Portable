package javax.microedition.lcdui;

public class Graphics {
    public static final int HCENTER = 1, VCENTER = 2, LEFT = 4, RIGHT = 8, TOP = 16, BOTTOM = 32, BASELINE = 64;
    public static final int SOLID = 0, DOTTED = 1;

    Image img;
    int tx, ty;
    int cx, cy, cw, ch;   // clip in absolute (untranslated) coordinates
    int color;
    int stroke;

    Graphics(Image i) {
        img = i;
        cw = i.width;
        ch = i.height;
    }

    public void translate(int x, int y) { tx += x; ty += y; }
    public int getTranslateX() { return tx; }
    public int getTranslateY() { return ty; }
    public void setColor(int rgb) { color = rgb & 0xFFFFFF; }
    public void setColor(int r, int g, int b) { color = ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF); }
    public int getColor() { return color; }
    public int getRedComponent() { return (color >> 16) & 0xFF; }
    public int getGreenComponent() { return (color >> 8) & 0xFF; }
    public int getBlueComponent() { return color & 0xFF; }
    public void setStrokeStyle(int s) { stroke = s; }
    public int getStrokeStyle() { return stroke; }
    public void setFont(Font f) {}
    public Font getFont() { return Font.getDefaultFont(); }

    public int getClipX() { return cx - tx; }
    public int getClipY() { return cy - ty; }
    public int getClipWidth() { return cw; }
    public int getClipHeight() { return ch; }
    public void setClip(int x, int y, int w, int h) {
        x += tx; y += ty;
        int x2 = x + w, y2 = y + h;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x2 > img.width) x2 = img.width;
        if (y2 > img.height) y2 = img.height;
        cx = x; cy = y;
        cw = x2 > x ? x2 - x : 0;
        ch = y2 > y ? y2 - y : 0;
    }
    public void clipRect(int x, int y, int w, int h) {
        x += tx; y += ty;
        int x2 = x + w, y2 = y + h;
        int ox2 = cx + cw, oy2 = cy + ch;
        if (x < cx) x = cx;
        if (y < cy) y = cy;
        if (x2 > ox2) x2 = ox2;
        if (y2 > oy2) y2 = oy2;
        cx = x; cy = y;
        cw = x2 > x ? x2 - x : 0;
        ch = y2 > y ? y2 - y : 0;
    }

    public native void fillRect(int x, int y, int w, int h);
    public void drawRect(int x, int y, int w, int h) {
        if (w < 0 || h < 0) return;
        fillRect(x, y, w + 1, 1);
        fillRect(x, y + h, w + 1, 1);
        fillRect(x, y, 1, h + 1);
        fillRect(x + w, y, 1, h + 1);
    }
    public native void drawLine(int x1, int y1, int x2, int y2);
    public native void fillArc(int x, int y, int w, int h, int start, int arc);
    public void drawArc(int x, int y, int w, int h, int start, int arc) { fillArc(x, y, w, h, start, arc); }
    public void fillRoundRect(int x, int y, int w, int h, int aw, int ah) { fillRect(x, y, w, h); }
    public void drawRoundRect(int x, int y, int w, int h, int aw, int ah) { drawRect(x, y, w, h); }
    public native void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3);
    public void drawString(String s, int x, int y, int anchor) {}
    public void drawChar(char c, int x, int y, int anchor) {}
    public void drawSubstring(String s, int off, int len, int x, int y, int anchor) {}

    public void drawImage(Image src, int x, int y, int anchor) {
        int w = src.width, h = src.height;
        if ((anchor & HCENTER) != 0) x -= w / 2;
        else if ((anchor & RIGHT) != 0) x -= w;
        if ((anchor & VCENTER) != 0) y -= h / 2;
        else if ((anchor & BOTTOM) != 0) y -= h;
        blit(src.pixels, src.width, 0, 0, w, h, 0, x, y);
    }
    public void drawRegion(Image src, int sx, int sy, int w, int h, int transform, int x, int y, int anchor) {
        if (sx < 0 || sy < 0 || w < 0 || h < 0 || sx + w > src.width || sy + h > src.height)
            throw new IllegalArgumentException();
        int dw = w, dh = h;
        if ((transform & 4) != 0) { dw = h; dh = w; }
        if ((anchor & HCENTER) != 0) x -= dw / 2;
        else if ((anchor & RIGHT) != 0) x -= dw;
        if ((anchor & VCENTER) != 0) y -= dh / 2;
        else if ((anchor & BOTTOM) != 0) y -= dh;
        blit(src.pixels, src.width, sx, sy, w, h, transform, x, y);
    }
    public native void drawRGB(int[] rgb, int offset, int scanlength, int x, int y, int w, int h, boolean alpha);
    public void copyArea(int sx, int sy, int w, int h, int dx, int dy, int anchor) {
        Image copy = Image.createImage(img, sx + tx, sy + ty, w, h, 0);
        drawImage(copy, dx, dy, anchor);
    }

    /** Draws a (possibly transformed) source region with its top-left at (x, y) after transform. */
    native void blit(int[] src, int srcWidth, int sx, int sy, int w, int h, int transform, int x, int y);
    static native void transformRegion(int[] src, int srcWidth, int sx, int sy, int w, int h, int transform, int[] out, int[] wh);
}
