package cc.pmtech;

import java.io.File;

import android.app.Activity;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.KeyEvent;

import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.graphics.SurfaceTexture;
import android.graphics.Canvas;

import static android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
import static android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
import static android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_LOW_PROFILE;

import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.MotionEvent;
import android.view.inputmethod.InputMethodManager;

import androidx.security.crypto.EncryptedSharedPreferences;
import android.content.SharedPreferences;
import androidx.security.crypto.MasterKey;

import android.widget.EditText;
import android.text.InputType;
import android.view.inputmethod.EditorInfo;
import android.os.Vibrator;
import android.os.VibrationEffect;

import org.fmod.FMOD;

// new graphics api agnostic implementation of android surface to support native egl
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
}

public class pen_activity extends Activity {
    public static native void register_asset_manager(AssetManager asset_manager);
    public static native void set_persistent_data_dir(String persistent_data_dir);
    public static native void set_cache_dir(String cache_dir);
    public static native void native_on_key_down(int key_code, int unicode_char);
    public static native void native_on_key_up(int key_code);
    public static native void native_back_button_pressed();
    private native void init(Activity activity);

    private static SurfaceWrapper m_surfaceView;
    private static Activity m_instance;
    private static Context m_context;
    private MasterKey m_masterKey;
    private SharedPreferences m_sharedPrefs;
    private boolean m_canUseSharedPrefs;

    public int getStatusBarHeight() {
        int result = 0;
        int resourceId = this.getResources().getIdentifier("status_bar_height", "dimen", "android");
        if (resourceId > 0) {
            result = this.getResources().getDimensionPixelSize(resourceId);
        }
        return (int)((float)result);
    }

    protected void loadLibs(String name) {
        System.loadLibrary(name);
        System.loadLibrary("fmod");
    }

    void initCredentials()
    {
        try {
            m_masterKey = new MasterKey.Builder(this)
                .setKeyScheme(MasterKey.KeyScheme.AES256_GCM)
                .build();

            m_sharedPrefs = EncryptedSharedPreferences.create(
                    this,
                    "diig_shared_prefs",
                    m_masterKey,
                    EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
                    EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM
            );

            m_canUseSharedPrefs = true;
        }
        catch (Exception e) {
            m_canUseSharedPrefs = false;
        }
    }

    @Override
	protected void onCreate(Bundle arg0) {

        m_instance = this;
        m_context = this;

        getWindow().setDecorFitsSystemWindows(false);
        getWindow().setStatusBarColor(android.graphics.Color.TRANSPARENT);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        init(this);
        FMOD.init(this);
        initCredentials();

        // register asset manager
        set_persistent_data_dir(this.getFilesDir().getPath());
        set_cache_dir(this.getCacheDir().getPath());
        register_asset_manager(getApplicationContext().getAssets());

        // setup view / surface
        m_surfaceView = new SurfaceWrapper(this);

        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(metrics);

        m_surfaceView.m_window_width = metrics.widthPixels;
        m_surfaceView.m_window_height = metrics.heightPixels;

        setContentView(m_surfaceView);

		super.onCreate(arg0);
	}

	@Override
	protected void onResume() {
		super.onResume();
	}

	@Override
	protected void onPause() {
		super.onPause();
        
        // kill the process when app is backgrounded
        android.os.Process.killProcess(android.os.Process.myPid());
        System.exit(0);
	}

    @Override
    protected void onStop() {
        super.onStop();
    }

	@Override
	protected void onDestroy()
    {
        super.onDestroy();
        FMOD.close();
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event)
    {
        switch(event.getKeyCode())
        {
            case android.view.KeyEvent.KEYCODE_BACK:
                native_back_button_pressed();
                return true;
            default:
                native_on_key_down(keyCode, event.getUnicodeChar());
                return false;
        }
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event)
    {
        native_on_key_up(keyCode);
        return false;
    }

    @Override
    public void onBackPressed()
    {
        native_back_button_pressed();
    }

    public static void showKeyboard(boolean show)
    {
        View view = m_surfaceView;

        Context context = view.getContext();

        EditText field = new EditText(context);
        field.setInputType(
            InputType.TYPE_CLASS_TEXT |
            InputType.TYPE_TEXT_VARIATION_PASSWORD |
            InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
        );
        field.setImeOptions(EditorInfo.IME_ACTION_DONE);
        field.requestFocus();

        if(show)
        {
            InputMethodManager mgr = (InputMethodManager)context.getSystemService(Context.INPUT_METHOD_SERVICE);
            mgr.toggleSoftInput(InputMethodManager.SHOW_FORCED,0);
        }
        else
        {
            InputMethodManager mgr = (InputMethodManager)context.getSystemService(Context.INPUT_METHOD_SERVICE);
            mgr.toggleSoftInput(InputMethodManager.HIDE_IMPLICIT_ONLY,0);
        }
    }

    public static void openURL(String url)
    {
        m_instance.runOnUiThread(() -> {
            Intent webIntent = new Intent(Intent.ACTION_VIEW, android.net.Uri.parse(url));
            m_instance.startActivity(webIntent);
        });
    }

    public static void createDirectory(String strdir)
    {
        File dir = new File(strdir);
        if (!dir.exists()) {
            dir.mkdirs(); 
        }
    }

    public static void deleteDirectory(String strdir) {
        File dir = new File(strdir);
        if (dir.isDirectory()) {
            File[] children = dir.listFiles();
            if (children != null) {
                for (File child : children) {
                    deleteDirectory(child.toString());
                }
            }
        }
        dir.delete();
    }

    public boolean setCredential(String key, String value)
    {
        if(m_canUseSharedPrefs)
        {
            m_sharedPrefs.edit().putString(key, value).apply();
            return true;
        }

        return false;
    }

    public String getCredential(String key)
    {
        if(m_canUseSharedPrefs)
        {
            return m_sharedPrefs.getString(key, "");
        }

        return "";
    }

    public void triggerVibration()
    {
        Vibrator vibrator = (Vibrator)m_context.getSystemService(Context.VIBRATOR_SERVICE);
        VibrationEffect effect = VibrationEffect.createOneShot(16, VibrationEffect.DEFAULT_AMPLITUDE);
        vibrator.cancel();
        vibrator.vibrate(effect);
    }
}
