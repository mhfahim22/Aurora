/* ════════════════════════════════════════════════════════════
   AuroraActivity — Android Activity hosting the Aurora runtime
   ════════════════════════════════════════════════════════════ */

package aurora;

import android.app.NativeActivity;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.view.inputmethod.InputMethodManager;
import android.content.pm.PackageManager;
import android.util.Log;

public class AuroraActivity extends NativeActivity {
    private static final String TAG = "Aurora";

    static {
        System.loadLibrary("aurora_runtime");
    }

    /* ── Native methods ── */

    private static native void nativeInit();
    private static native void nativeSetActivity(Object activity);
    private static native void nativeOnTouch(int action, int id, float x, float y, float pressure, float size);
    private static native void nativeOnKey(int keyCode, int pressed);
    private static native void nativeOnImeText(String text);
    private static native void nativeOnSensor(int type, float x, float y, float z);
    private static native void nativeOnPermissionResult(String permission, boolean granted);

    /* ── Lifecycle ── */

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        nativeSetActivity(this);
        nativeInit();
    }

    @Override
    protected void onStart() {
        super.onStart();
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    @Override
    protected void onStop() {
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }

    /* ── Touch Input ── */

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

    /* ── Keyboard Input ── */

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

    /* ── Permissions ── */

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        for (int i = 0; i < permissions.length && i < grantResults.length; i++) {
            nativeOnPermissionResult(permissions[i], grantResults[i] == PackageManager.PERMISSION_GRANTED);
        }
    }
}
