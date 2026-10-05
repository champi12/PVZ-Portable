package javax.microedition.lcdui.game;

import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;

public abstract class GameCanvas extends Canvas {
    public static final int UP_PRESSED = 1 << UP, DOWN_PRESSED = 1 << DOWN, LEFT_PRESSED = 1 << LEFT;
    public static final int RIGHT_PRESSED = 1 << RIGHT, FIRE_PRESSED = 1 << FIRE;
    private Image buffer;

    protected GameCanvas(boolean suppressKeyEvents) {}
    protected Graphics getGraphics() {
        if (buffer == null) buffer = Image.createImage(getWidth(), getHeight());
        return buffer.getGraphics();
    }
    public int getKeyStates() { return 0; }
    public void paint(Graphics g) {
        if (buffer != null) g.drawImage(buffer, 0, 0, Graphics.TOP | Graphics.LEFT);
    }
    public void flushGraphics() { repaint(); }
    public void flushGraphics(int x, int y, int w, int h) { repaint(); }
}
