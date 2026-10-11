package java.lang;

import java.io.InputStream;
import java.io.ByteArrayInputStream;

public final class Class {
    private long vmClass;   // JClass* of the runtime

    private Class() {}
    public native String getName();
    private static native byte[] loadResource(String name);

    public InputStream getResourceAsStream(String name) {
        if (name == null) throw new NullPointerException();
        if (name.length() > 0 && name.charAt(0) == '/') name = name.substring(1, name.length());
        byte[] data = loadResource(name);
        if (data == null) return null;
        return new ByteArrayInputStream(data);
    }
}
