package no.pack.drill.ararch

import android.os.Bundle
import android.util.Log
import android.util.Size
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import no.pack.drill.ararch.mar.HardwareCamera
import no.pack.drill.ararch.mar.MAR
import java.lang.StringBuilder

class ARActivity : AppCompatActivity()
//====================================
{
   companion object
   {
      private val TAG = ARActivity::class.java.simpleName
   }

   private var surfaceHeight: Int = -1
   private var surfaceWidth: Int = -1
   private lateinit var activeCameras: HashMap<String, Size>
   private var aprilTagsOnOff = false
   private var faceRecogOnOff = false
   private lateinit var surface: SurfaceView

   fun getSurface() : Surface { return surface.holder.surface }

   override fun onCreate(savedInstanceState: Bundle?)
   //------------------------------------------------
   {
      super.onCreate(savedInstanceState)
      setContentView(R.layout.activity_ar)
      surface = findViewById(R.id.surfaceView)
      surface.holder.addCallback(object : SurfaceHolder.Callback
      {
         override fun surfaceCreated(holder: SurfaceHolder) {}

         override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int)
         //-------------------------------------------------------------------------------------------
         {
            //val rotation = (getSystemService(Context.WINDOW_SERVICE) as WindowManager).defaultDisplay.rotation
            Log.i(TAG, "Surface using format : ${HardwareCamera.imageFormatToString(format)}")
            surfaceWidth = surface.holder.surfaceFrame.width()
            surfaceHeight = surface.holder.surfaceFrame.height()
            if (! MAR.setSurface(surface.holder.surface, surfaceWidth, surfaceHeight))
            {
               MainActivity.message(this@ARActivity, "Surface initialization failed",
                  isYesNo = true, onFinish = object : MainActivity.MessageDialogCommand()
                  { override fun run()
                  {
                     this@ARActivity.setResult(0)
                     this@ARActivity.finish()
                  }
                  })
            }
            val errbuf = StringBuilder()
            if (MAR.startPreview(this@ARActivity, activeCameras, errbuf))
               MAR.startMAR(MainActivity.RENDERER_TYPE, aprilTagsOnOff, faceRecogOnOff)
            else
            {
               MainActivity.message(this@ARActivity, errbuf.toString(), isYesNo = true,
                  onFinish =  object : MainActivity.MessageDialogCommand()
                  { override fun run()
                  {
                     this@ARActivity.setResult(0)
                     this@ARActivity.finish()
                  }
                  })
            }
         }

         override fun surfaceDestroyed(holder: SurfaceHolder) = MAR.stopPreview(activeCameras.keys.toMutableList())
      })

      activeCameras = intent.extras?.getSerializable("cameras") as HashMap<String, Size>
      aprilTagsOnOff = intent.extras?.getBoolean("aprilTags")!!
      faceRecogOnOff  = intent.extras?.getBoolean("faceRecog")!!
   }

   override fun onPause()
   //--------------------
   {
      super.onPause()
      if (activeCameras.isNotEmpty())
         MAR.stopPreview(activeCameras.keys.toMutableList())

   }

   fun onStop(vw : View?)
   //-------------------------
   {
      if (activeCameras.isNotEmpty())
         MAR.stopPreview(activeCameras.keys.toMutableList())
      MAR.stopMAR()
      setResult(1)
      finish()
   }
}
