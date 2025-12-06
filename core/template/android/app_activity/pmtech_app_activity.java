package pmtech.app.android;

import cc.pmtech.pen_activity;
import com.pmtech.examples.R;
import android.os.Bundle;

public class pmtech_app_activity extends pen_activity
{
    @Override
	protected void onCreate(Bundle arg0) 
    {
        String lib = getString(R.string.app_name);
        loadLibs(lib);

        super.onCreate(arg0);
    }
}