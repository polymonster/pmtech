package cc.pmtech;

import android.app.Activity;
import android.os.Bundle;
import android.view.KeyEvent;
import android.util.Log;

/*
class SurfaceWrapper extends SurfaceView
        implements SurfaceHolder.Callback, SurfaceTexture.OnFrameAvailableListener{
*/

public class pen_activity extends Activity {

	public static native void entry();

	static {
		System.loadLibrary("basic_triangle");
	}

	@Override
	protected void onCreate(Bundle arg0) {
		Log.d("PMTECH", "hello world");

		// create surface

		entry();

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
	protected void onDestroy() {
		super.onDestroy();
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
