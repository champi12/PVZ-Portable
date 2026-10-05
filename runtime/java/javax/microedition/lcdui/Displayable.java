package javax.microedition.lcdui;

public abstract class Displayable {
    Displayable() {}
    public int getWidth() { return Display.screenWidth(); }
    public int getHeight() { return Display.screenHeight(); }
    public boolean isShown() { return Display.instance != null && Display.instance.current == this; }
    void showNotify0() {}
    void hideNotify0() {}
}
