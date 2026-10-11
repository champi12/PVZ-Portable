package java.lang;

public class Object {
    public Object() {}
    public final native Class getClass();
    public native int hashCode();
    public boolean equals(Object o) { return this == o; }
    public String toString() { return getClass().getName() + "@" + Integer.toHexString(hashCode()); }
}
