package javax.microedition.media;

import java.io.InputStream;
import java.io.IOException;

public final class Manager {
    private Manager() {}
    public static Player createPlayer(InputStream in, String type) throws IOException, MediaException {
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
        byte[] data = new byte[n];
        System.arraycopy(buf, 0, data, 0, n);
        return new PlayerImpl(data, type);
    }
    public static Player createPlayer(String locator) throws IOException, MediaException {
        return new PlayerImpl(null, "tone");
    }
    public static String[] getSupportedContentTypes(String protocol) {
        String[] s = new String[1];
        s[0] = "audio/midi";
        return s;
    }
    public static void playTone(int note, int duration, int volume) {}
}
