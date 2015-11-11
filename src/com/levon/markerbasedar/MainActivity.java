package com.levon.markerbasedar;

import java.io.IOException;
import android.app.Activity;
import android.graphics.Point;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.hardware.Camera.PreviewCallback;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.view.Gravity;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceHolder.Callback;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.TextureView.SurfaceTextureListener;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;
import com.levon.markerbasedar.R;

public class MainActivity extends Activity implements Callback {

private Camera mCamera;

private SurfaceTexture surfaceTexture;
private boolean mInitialized = false;

@Override
protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

	//Remove title bar
	requestWindowFeature(Window.FEATURE_NO_TITLE);

	//Remove notification bar
	getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
	
	surfaceTexture = new SurfaceTexture(10);
	
	mCamera = Camera.open();
    mCamera.setPreviewCallback(new PreviewCallback() {
        public void onPreviewFrame(byte[] data, Camera camera) {
        	synchronized (MainActivity.this) {
		    	if (mInitialized)
		    	{
                    Camera.Parameters parameters = camera.getParameters();
                    int width = parameters.getPreviewSize().width;
                    int height = parameters.getPreviewSize().height;
                    
                    ProcessFrame(width, height, data);
		    	}
        	}
        }
    });
    
	try {
		mCamera.setPreviewTexture(surfaceTexture);
	} catch (IOException e) {
		// TODO Auto-generated catch block
		e.printStackTrace();
	}
    mCamera.startPreview();
    
    
    setContentView(R.layout.activity_main);
    SurfaceView surfaceView = (SurfaceView)findViewById(R.id.surfaceview);
    surfaceView.getHolder().addCallback(this);
}

@Override
public void surfaceCreated(SurfaceHolder holder) {
	Initialize(holder.getSurface());
	mInitialized = true;
}

@Override
public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
	// TODO Auto-generated method stub
	
}

@Override
public void surfaceDestroyed(SurfaceHolder holder) {
	// TODO Auto-generated method stub
	
}

//public native void Initialize();

public native void Initialize(Surface surface);

public native void ProcessFrame(int width, int height, byte yuv[]);

static {
	System.loadLibrary("MarkerBasedAR");
}

}