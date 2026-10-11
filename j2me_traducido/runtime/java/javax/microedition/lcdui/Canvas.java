package javax.microedition.lcdui;

public abstract class Canvas extends Displayable {
    public static final int UP = 1, DOWN = 6, LEFT = 2, RIGHT = 5, FIRE = 8;
    public static final int GAME_A = 9, GAME_B = 10, GAME_C = 11, GAME_D = 12;
    public static final int KEY_NUM0 = 48, KEY_NUM1 = 49, KEY_NUM2 = 50, KEY_NUM3 = 51, KEY_NUM4 = 52;
    public static final int KEY_NUM5 = 53, KEY_NUM6 = 54, KEY_NUM7 = 55, KEY_NUM8 = 56, KEY_NUM9 = 57;
    public static final int KEY_STAR = 42, KEY_POUND = 35;

    boolean repaintPending = true;

    protected Canvas() {}
    protected abstract void paint(Graphics g);
    public final void repaint() { repaintPending = true; }
    public final void repaint(int x, int y, int w, int h) { repaintPending = true; }
    public final void serviceRepaints() {}
    public void setFullScreenMode(boolean mode) {}
    public boolean hasPointerEvents() { return true; }
    public boolean hasPointerMotionEvents() { return true; }
    public boolean hasRepeatEvents() { return false; }
    public boolean isDoubleBuffered() { return true; }

    public int getGameAction(int keyCode) {
        switch (keyCode) {
            case -1: case KEY_NUM2: return UP;
            case -2: case KEY_NUM8: return DOWN;
            case -3: case KEY_NUM4: return LEFT;
            case -4: case KEY_NUM6: return RIGHT;
            case -5: case KEY_NUM5: return FIRE;
            case KEY_NUM1: return GAME_A;
            case KEY_NUM3: return GAME_B;
            case KEY_NUM7: return GAME_C;
            case KEY_NUM9: return GAME_D;
        }
        return 0;
    }
    public int getKeyCode(int gameAction) {
        switch (gameAction) {
            case UP: return -1;
            case DOWN: return -2;
            case LEFT: return -3;
            case RIGHT: return -4;
            case FIRE: return -5;
        }
        return 0;
    }
    public String getKeyName(int keyCode) { return String.valueOf(keyCode); }

    protected void keyPressed(int keyCode) {}
    protected void keyReleased(int keyCode) {}
    protected void keyRepeated(int keyCode) {}
    protected void pointerPressed(int x, int y) {}
    protected void pointerReleased(int x, int y) {}
    protected void pointerDragged(int x, int y) {}
    protected void showNotify() {}
    protected void hideNotify() {}
    protected void sizeChanged(int w, int h) {}

    void showNotify0() { showNotify(); }
    void hideNotify0() { hideNotify(); }
}
