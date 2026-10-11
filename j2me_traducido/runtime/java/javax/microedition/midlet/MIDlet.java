package javax.microedition.midlet;

public abstract class MIDlet {
    protected MIDlet() {}
    protected abstract void startApp() throws MIDletStateChangeException;
    protected abstract void pauseApp();
    protected abstract void destroyApp(boolean unconditional) throws MIDletStateChangeException;

    public final String getAppProperty(String key) {
        if ("MIDlet-Version".equals(key)) return "4.6.0";
        if ("MIDlet-Name".equals(key)) return "Plants vs Zombies";
        if ("MIDlet-Vendor".equals(key)) return "Electronic Arts Inc.";
        return null;
    }
    public final void notifyDestroyed() { exitRequested = true; }
    public final void notifyPaused() {}
    public final void resumeRequest() {}
    public final boolean platformRequest(String url) { return false; }

    /** Set when the application asked to quit; polled by the runtime main loop. */
    public static boolean exitRequested;

    /** Runtime entry points (the platform layer cannot call protected methods directly). */
    public static void __start(MIDlet m) {
        try { m.startApp(); } catch (MIDletStateChangeException e) {}
    }
    public static void __pause(MIDlet m) { m.pauseApp(); }
    public static void __destroy(MIDlet m) {
        try { m.destroyApp(true); } catch (MIDletStateChangeException e) {}
    }
}
