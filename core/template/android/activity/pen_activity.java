package cc.pmtech;

import com.pmtech.examples.R;

import android.app.Activity;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.KeyEvent;
import android.util.Log;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.graphics.SurfaceTexture;
import android.graphics.Canvas;

import android.graphics.SurfaceTexture;
import android.graphics.Canvas;

import static android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
import static android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
import static android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_LOW_PROFILE;

import android.view.KeyEvent;
import android.view.OrientationEventListener;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.MotionEvent;
import android.view.Display;
import android.view.GestureDetector;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;

import org.fmod.FMOD;

class VideoSurfaceTexture extends SurfaceTexture
{
    VideoSurfaceTexture(int name)
    {
        super(name);
    }

    public long m_playerPointer = 0;
};

// new graphics api agnostic implementation of android surface to support native egl and vulkan
class SurfaceWrapper extends SurfaceView implements SurfaceHolder.Callback {

	public static native void surface_created(Surface surface, int window_width, int window_height, int display_width, int display_height, int orientation, long app_ptr);
    public static native void surface_changed(int width, int height);
	public static native void render(SurfaceWrapper caller);
    public static native void on_touch_down(int id, float x, float y, float pressure, float majoraxis, float minoraxis, float angle);
    public static native void on_touch_up(int id, float x, float y, float pressure, float majoraxis, float minoraxis, float angle);
    public static native void on_touch_moved(int id, float x, float y, float pressure, float majoraxis, float minoraxis, float angle);
    public static native void on_touch_cancelled(int id, float x, float y);

    public int m_display_width;   // size in pixels of the physical device's screen
    public int m_display_height;
    public int m_window_width;    // size in pixels of the renderable area
    public int m_window_height;
    public int orientation; // orientation of device at startup
    public int visibility;

    private static long app_ptr = 0;
    private Context m_context;

    public SurfaceWrapper(Context context)
    {
        super(context);

        getHolder().addCallback(this);
        m_context = context;
    }

    @Override
    protected void onDraw(Canvas canvas)
    {
        render(this);
        invalidate();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder)
    {
        setWillNotDraw(false);

        Surface surf = holder.getSurface();
        surface_created(surf, m_window_width, m_window_height, m_display_width, m_display_height, orientation, app_ptr);
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
    {
        surface_changed(width, height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder)
    {

    }

    @Override
    public boolean onTouchEvent(MotionEvent event)
    {
        final int action = event.getActionMasked();
        final int pointerIndex = event.getActionIndex();
        final int pointerId = event.getPointerId(pointerIndex);

        switch(action)
        {
            case MotionEvent.ACTION_MOVE:
                for(int j=0; j<event.getPointerCount(); j++)
                {
                    on_touch_moved(event.getPointerId(j),
                            event.getX(j),
                            event.getY(j),
                            event.getPressure(j),
                            event.getTouchMajor(j),
                            event.getTouchMinor(j),
                            event.getOrientation(j));
                }
                break;

            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_UP:
                on_touch_up(pointerId,
                        event.getX(pointerIndex),
                        event.getY(pointerIndex),
                        event.getPressure(pointerIndex),
                        event.getTouchMajor(pointerIndex),
                        event.getTouchMinor(pointerIndex),
                        event.getOrientation(pointerIndex));
                break;

            case MotionEvent.ACTION_POINTER_DOWN:
            case MotionEvent.ACTION_DOWN:
                on_touch_down(pointerId,
                        event.getX(pointerIndex),
                        event.getY(pointerIndex),
                        event.getPressure(pointerIndex),
                        event.getTouchMajor(pointerIndex),
                        event.getTouchMinor(pointerIndex),
                        event.getOrientation(pointerIndex));
                break;

            case MotionEvent.ACTION_CANCEL:
                on_touch_cancelled(pointerId, event.getX(), event.getY());
                break;
        }

        return true;
    }

    public enum e_vibrationType
    {
        VIBRATION_CHOICE_BEGIN,
        VIBRATION_CHOICE_SELECTED,
        VIBRATION_CHOICE_ENDED,
        VIBRATION_CHOICE_CANCEL
    }

    public void triggerVibration(int vibrationType)
    {
        /*
        e_vibrationType vibration = e_vibrationType.values()[vibrationType];

        Vibrator vibrator = (Vibrator)m_context.getSystemService(Context.VIBRATOR_SERVICE);

        switch(vibration)
        {
            case VIBRATION_CHOICE_SELECTED:
                VibrationEffect vibrationEffect;
                vibrationEffect =  VibrationEffect.createOneShot(16, VibrationEffect.DEFAULT_AMPLITUDE);
                vibrator.cancel();
                vibrator.vibrate(vibrationEffect);
                break;
            default:
                break;
        }
        */
    }

    public void triggerVibration(float amplitude, float duration)
    {
        /*
        Vibrator vibrator = (Vibrator)m_context.getSystemService(Context.VIBRATOR_SERVICE);

        VibrationEffect vibrationEffect;
        int effectAmplitude = (int) (amplitude * 255);
        if(effectAmplitude > 0)
        {
            long effectDuration = duration > 0 ? (long) (duration * 1000) : 16;
            vibrationEffect = VibrationEffect.createOneShot(effectDuration, effectAmplitude);
            vibrator.vibrate(vibrationEffect);
        }
        */
    }

    public boolean isNetworkConnected()
    {
        /*
        ConnectivityManager connectivity = (ConnectivityManager)m_context.getSystemService(Context.CONNECTIVITY_SERVICE);
        if(connectivity != null)
        {
            NetworkInfo networkInfo = connectivity.getActiveNetworkInfo();
            if(networkInfo != null)
            {
                return networkInfo.isConnected();
            }
        }
        */

        return false;
    }
}

public class pen_activity extends Activity {

	public static native void entry();
    public static native void register_asset_manager(AssetManager asset_manager);

    void set_immersive_mode()
    {
        int vis = SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
        vis |= SYSTEM_UI_FLAG_LOW_PROFILE | SYSTEM_UI_FLAG_HIDE_NAVIGATION | SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        getWindow().getDecorView().setSystemUiVisibility(vis);
    }

    private native void initFMOD(Activity activity);

    @Override
	protected void onCreate(Bundle arg0) {
        // load lib
        String lib = getString(R.string.app_name);
        Log.d("PMTECH", lib);
        System.loadLibrary(lib);

        System.loadLibrary("fmod");

        initFMOD(this);
        FMOD.init(this);

        entry();

        // register asset manager
        register_asset_manager(getApplicationContext().getAssets());

        // setup view / surface
        set_immersive_mode();
        SurfaceWrapper view = new SurfaceWrapper(this);

        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);

        view.m_window_width = metrics.widthPixels;
        view.m_window_height = metrics.heightPixels;

        setContentView(view);

		super.onCreate(arg0);
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
	protected void onDestroy()
    {
        super.onDestroy();
        FMOD.close();

    }

	@Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
		return false;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
		return false;
    }
}
