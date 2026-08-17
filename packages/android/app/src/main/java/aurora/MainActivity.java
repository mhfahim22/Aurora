package aurora;

import android.app.NativeActivity;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.content.pm.PackageManager;
import android.util.Log;

public class MainActivity extends NativeActivity {
    private static final String TAG = "AuroraApp";

    static { System.loadLibrary("aurora_app"); }

    private static native void nativeInit();
    private static native void nativeInitRenderer();
    private static native void nativeOnTouch(int action, int id, float x, float y, float pressure, float size);
    private static native void nativeOnKey(int keyCode, int pressed);
    private static native void nativeOnImeText(String text);
    private static native void nativeOnPermissionResult(String permission, boolean granted);
    private static native void nativeOnSafeArea(float top, float bottom, float left, float right);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        nativeInit();
        nativeInitRenderer();
        getWindow().getDecorView().post(this::reportSafeArea);
    }

    /* 37.3 — read real device insets (status bar / notch, navigation bar
       / home indicator) and push them to the native widget tree in dp. */
    private void reportSafeArea() {
        try {
            View decor = getWindow().getDecorView();
            if (decor == null) return;
            android.graphics.Rect frame = new android.graphics.Rect();
            decor.getWindowVisibleDisplayFrame(frame);
            android.view.WindowInsets insets = decor.getRootWindowInsets();
            if (insets == null) return;
            float density = getResources().getDisplayMetrics().density;
            if (density <= 0.0f) density = 1.0f;
            float top = insets.getSystemWindowInsetTop() / density;
            float bottom = insets.getSystemWindowInsetBottom() / density;
            float left = insets.getSystemWindowInsetLeft() / density;
            float right = insets.getSystemWindowInsetRight() / density;
            nativeOnSafeArea(top, bottom, left, right);
        } catch (Exception e) {
            Log.w(TAG, "reportSafeArea failed: " + e.getMessage());
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) reportSafeArea();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        int pointerIndex = event.getActionIndex();
        int id = event.getPointerId(pointerIndex);
        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                nativeOnTouch(0, id, event.getX(pointerIndex), event.getY(pointerIndex),
                        event.getPressure(pointerIndex), event.getSize(pointerIndex));
                return true;
            case MotionEvent.ACTION_MOVE:
                for (int i = 0; i < event.getPointerCount(); i++) {
                    nativeOnTouch(2, event.getPointerId(i), event.getX(i), event.getY(i),
                            event.getPressure(i), event.getSize(i));
                }
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                nativeOnTouch(1, id, event.getX(pointerIndex), event.getY(pointerIndex),
                        event.getPressure(pointerIndex), event.getSize(pointerIndex));
                return true;
            case MotionEvent.ACTION_CANCEL:
                nativeOnTouch(3, id, 0, 0, 0, 0);
                return true;
        }
        return super.onTouchEvent(event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        nativeOnKey(event.getKeyCode(), 1);
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        nativeOnKey(event.getKeyCode(), 0);
        return super.onKeyUp(keyCode, event);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        for (int i = 0; i < permissions.length && i < grantResults.length; i++) {
            nativeOnPermissionResult(permissions[i], grantResults[i] == PackageManager.PERMISSION_GRANTED);
        }
    }
}
