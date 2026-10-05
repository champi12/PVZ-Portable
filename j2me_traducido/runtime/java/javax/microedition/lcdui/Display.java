package javax.microedition.lcdui;

import javax.microedition.midlet.MIDlet;

public class Display {
    static Display instance;
    Displayable current;
    private Displayable pendingShow;
    private Runnable[] serial = new Runnable[8];
    private int serialCount;
    private static Image screen;
    private static int[] events = new int[3 * 64];

    // Event types delivered by the platform layer.
    static final int EV_KEY_DOWN = 1, EV_KEY_UP = 2, EV_PTR_DOWN = 3, EV_PTR_UP = 4, EV_PTR_DRAG = 5;
    static final int EV_PAUSE = 6, EV_RESUME = 7;

    private Display() {}

    public static Display getDisplay(MIDlet m) {
        if (instance == null) instance = new Display();
        return instance;
    }
    public Displayable getCurrent() { return current; }
    public void setCurrent(Displayable d) {
        if (d == current) return;
        if (current != null) current.hideNotify0();
        current = d;
        pendingShow = d;
        if (d instanceof Canvas) ((Canvas)d).repaintPending = true;
    }
    public void callSerially(Runnable r) {
        if (serialCount == serial.length) {
            Runnable[] n = new Runnable[serialCount * 2];
            System.arraycopy(serial, 0, n, 0, serialCount);
            serial = n;
        }
        serial[serialCount++] = r;
    }
    public boolean vibrate(int ms) { return false; }
    public boolean flashBacklight(int ms) { return false; }
    public boolean isColor() { return true; }
    public int numColors() { return 1 << 24; }

    static native int screenWidth();
    static native int screenHeight();
    private static native int pollEvents(int[] buf);
    private static native void present(int[] pixels, int w, int h);

    /** Called by the runtime main loop once per iteration; returns false to quit. */
    public static boolean __tick(MIDlet midlet) {
        if (screen == null) screen = Image.createImage(screenWidth(), screenHeight());
        Display d = instance;
        int n = pollEvents(events);
        if (d != null) {
            for (int i = 0; i < n; i++) {
                int t = events[i * 3], a = events[i * 3 + 1], b = events[i * 3 + 2];
                if (t == EV_PAUSE) { MIDlet.__pause(midlet); continue; }
                if (t == EV_RESUME) { MIDlet.__start(midlet); continue; }
                Displayable cur = d.current;
                if (!(cur instanceof Canvas)) continue;
                Canvas c = (Canvas)cur;
                switch (t) {
                    case EV_KEY_DOWN: c.keyPressed(a); break;
                    case EV_KEY_UP: c.keyReleased(a); break;
                    case EV_PTR_DOWN: c.pointerPressed(a, b); break;
                    case EV_PTR_UP: c.pointerReleased(a, b); break;
                    case EV_PTR_DRAG: c.pointerDragged(a, b); break;
                }
            }
            if (d.pendingShow != null) {
                Displayable s = d.pendingShow;
                d.pendingShow = null;
                s.showNotify0();
            }
            int count = d.serialCount;
            if (count > 0) {
                Runnable[] run = new Runnable[count];
                System.arraycopy(d.serial, 0, run, 0, count);
                System.arraycopy(d.serial, count, d.serial, 0, d.serialCount - count);
                for (int i = d.serialCount - count; i < d.serialCount; i++) d.serial[i] = null;
                d.serialCount -= count;
                for (int i = 0; i < count; i++) run[i].run();
            }
            Displayable cur = d.current;
            if (cur instanceof Canvas && ((Canvas)cur).repaintPending) {
                Canvas c = (Canvas)cur;
                c.repaintPending = false;
                Graphics g = screen.getGraphics();
                c.paint(g);
                present(screen.pixels, screen.width, screen.height);
            }
        }
        return !MIDlet.exitRequested;
    }
}
