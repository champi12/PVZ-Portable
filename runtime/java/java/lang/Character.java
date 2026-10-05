package java.lang;

public final class Character {
    private Character() {}
    public static char toUpperCase(char c) {
        if ((c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7)) return (char)(c - 32);
        return c;
    }
    public static char toLowerCase(char c) {
        if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7)) return (char)(c + 32);
        return c;
    }
    public static boolean isDigit(char c) { return c >= '0' && c <= '9'; }
}
