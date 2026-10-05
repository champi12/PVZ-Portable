package javax.microedition.lcdui;

public final class Font {
    public static final int FACE_SYSTEM = 0, STYLE_PLAIN = 0, SIZE_MEDIUM = 0, SIZE_SMALL = 8, SIZE_LARGE = 16;
    private static Font def;
    private Font() {}
    public static Font getDefaultFont() {
        if (def == null) def = new Font();
        return def;
    }
    public static Font getFont(int face, int style, int size) { return getDefaultFont(); }
    public int getHeight() { return 12; }
    public int stringWidth(String s) { return s.length() * 6; }
    public int charWidth(char c) { return 6; }
}
