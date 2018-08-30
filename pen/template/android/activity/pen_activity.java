package cc.pmtech;

import android.app.Activity;
import android.os.Bundle;
import android.view.KeyEvent;
import android.util.Log;

public class pen_activity extends Activity{

    //public static native int ploop();

	static {
		System.loadLibrary("pen");
		System.loadLibrary("put");
	}

	@Override
	protected void onCreate(Bundle arg0) {
		Log.d("hello world", "test");

		//ploop();

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
