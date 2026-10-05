package javax.microedition.media;

import javax.microedition.media.control.VolumeControl;

final class PlayerImpl implements Player, VolumeControl {
    private int handle = -1;
    private int state = UNREALIZED;
    private int loops = 1;
    private int level = 100;
    private boolean muted;
    private String type;

    PlayerImpl(byte[] data, String type) {
        this.type = type;
        if (data != null && "audio/midi".equals(type)) handle = load(data);
    }

    private static native int load(byte[] data);
    private static native void play(int handle, int loops);
    private static native void halt(int handle);
    private static native void free(int handle);
    private static native boolean playing(int handle);
    private static native void volume(int handle, int level);

    public void realize() { if (state == UNREALIZED) state = REALIZED; }
    public void prefetch() { realize(); if (state == REALIZED) state = PREFETCHED; }
    public void start() {
        prefetch();
        if (handle >= 0) {
            volume(handle, muted ? 0 : level);
            play(handle, loops);
        }
        state = STARTED;
    }
    public void stop() {
        if (state == STARTED) {
            if (handle >= 0) halt(handle);
            state = PREFETCHED;
        }
    }
    public void deallocate() { stop(); if (state == PREFETCHED) state = REALIZED; }
    public void close() {
        stop();
        if (handle >= 0) free(handle);
        handle = -1;
        state = CLOSED;
    }
    public int getState() {
        if (state == STARTED && (handle < 0 || !playing(handle))) state = PREFETCHED;
        return state;
    }
    public void setLoopCount(int count) { loops = count; }
    public long getDuration() { return -1; }
    public long getMediaTime() { return 0; }
    public long setMediaTime(long t) { return 0; }
    public String getContentType() { return type; }
    public Control[] getControls() { Control[] c = new Control[1]; c[0] = this; return c; }
    public Control getControl(String t) {
        if (t.equals("VolumeControl") || t.equals("javax.microedition.media.control.VolumeControl")) return this;
        return null;
    }
    public void setMute(boolean m) { muted = m; if (handle >= 0) volume(handle, m ? 0 : level); }
    public boolean isMuted() { return muted; }
    public int setLevel(int l) {
        level = l < 0 ? 0 : (l > 100 ? 100 : l);
        if (handle >= 0 && !muted) volume(handle, level);
        return level;
    }
    public int getLevel() { return level; }
}
