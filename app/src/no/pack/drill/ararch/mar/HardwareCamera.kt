package no.pack.drill.ararch.mar

import android.content.Context
import android.graphics.ImageFormat
import android.graphics.PixelFormat
import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.os.*
import android.renderscript.RenderScript
import android.util.Log
import android.util.Range
import android.util.Size
import android.view.Surface
import java.util.concurrent.Semaphore
import java.util.concurrent.atomic.AtomicBoolean

enum class ColorFormats{ RGBA, BGRA }

interface CameraPreviewable
{
   fun onPreviewResult(cameraId: String, isPreviewing: Boolean, message: String)
}

interface SurfaceProvidable
{
   fun getSurfaces(): MutableList<Surface?>
}

//class HandlerExecutor(val handler: Handler) : Executor
//{
//   override fun execute(command: Runnable?)
//   {
//      if (!handler.post(command))
//      {
//         throw Exception("$handler is shutting down")
//      }
//   }
//}

abstract class HardwareCamera(protected val context: Context, protected val id: String,
                              protected val colorFormat: ColorFormats,
                              protected val isMultiCamera: Boolean,
                              protected val renderscript: RenderScript?)
//==============================================================================
{
   var manager: CameraManager? = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager?

   external fun addCamera(cameraId: String, queueSize: Int, isRearFacing: Boolean): Boolean
   external fun setPreviewSize(id: String, w: Int, h: Int): Boolean
   external fun enqueue(id: String, isRGBA: Boolean, timestamp: Long,
                        rgbaSize: Int, rgbaData: ByteArray,
                        greySize: Int, greyData: ByteArray?): Boolean
   external fun enqueueYUV(cameraId: String, YUV: ByteArray, w: Int, h: Int, isRGBA: Boolean,
                           timestamp: Long, rgbaSize: Int, rgbaData: ByteArray,
                        greySize: Int, greyData: ByteArray?): Boolean
   external fun clearQueue(cameraId: String)  : Boolean
   external fun inFlight(): Int

   protected var cameraDevice: CameraDevice? = null
   protected var stateCallback: CameraDevice.StateCallback? = null
   protected var captureSession: CameraCaptureSession? = null
   protected var captureId: Int = -1
   protected var standardFps: Array<Range<Int>>? = null
   protected var highSpeedFps: Array<Range<Int>>? = null
   protected var cameraWidth: Int = 0
   val width: Int
      get()  = cameraWidth
   protected var cameraHeight: Int = 0
   val height: Int
      get()  = cameraHeight
   val cameraId: String
      get() = id
   var queueSize: Int = 0

   protected var isConfigured: AtomicBoolean = AtomicBoolean(false)
//      private set
//      get() = isConfigured
   fun isConfigured(): Boolean { return isConfigured.get() }
   protected var stopping: Boolean = false
   val isStopping:Boolean
      get() = stopping
   var hasYUV: Boolean = false
      private set
   var hasNV21: Boolean = false
      private set
   var has565: Boolean = false
      private set
   var hasRGBA: Boolean = false
      private set
   var hasRGB: Boolean = false
      private set
   var hasJPG: Boolean = false
      private set
   var hasHighSpeed: Boolean =false
      private set
   var isRearFacing: Boolean = true
      private set
   protected var previewCallback: CameraPreviewable? = null
   protected val openCloseLock = Semaphore(1)

   fun open(): Boolean
   //-----------------
   {
      try
      {
         if (manager == null)
         {
            try
            { manager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager? }
            catch (e: java.lang.Exception)
            {
               Log.e(TAG, "HardwareCamera.open: CameraManager null")
               return false
            }
         }
         val characteristics = manager!!.getCameraCharacteristics(id)
         isRearFacing = characteristics.get(CameraCharacteristics.LENS_FACING) == CameraCharacteristics.LENS_FACING_BACK
         val caps = characteristics.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
         if (caps != null)
            hasHighSpeed = caps.filter { it -> it == CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_CONSTRAINED_HIGH_SPEED_VIDEO }.isNotEmpty()
         standardFps = characteristics.get<Array<Range<Int>>>(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES)
         val streamConfig = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)
         if (streamConfig != null)
         {
            if (hasHighSpeed)
               highSpeedFps = streamConfig.highSpeedVideoFpsRanges
            val outputs = streamConfig.outputFormats
            if (outputs != null)
            {
               for (c in outputs)
               {
                  Log.i(TAG, "Supported output format: $c - ${imageFormatToString(c)}")
                  when (c)
                  {
                     ImageFormat.YUV_420_888 -> hasYUV = true
                     ImageFormat.NV21 -> hasNV21 = true
                     ImageFormat.RGB_565 -> has565 = true
                     ImageFormat.FLEX_RGBA_8888 -> hasRGBA = true
                     ImageFormat.FLEX_RGB_888 -> hasRGB = true
                     ImageFormat.JPEG -> hasJPG = true
                  }
               }
            }
         }
         else
         {
            hasYUV = false
            hasNV21 = true
         }
         if (!hasYUV && !hasNV21)
         {
            hasYUV = streamConfig!!.getOutputSizes(ImageFormat.YUV_420_888) != null
            hasNV21 = streamConfig.getOutputSizes(ImageFormat.NV21) != null
         }
//         var imageFormat = if (hasYUV) ImageFormat.YUV_420_888 else if (hasNV21) ImageFormat.NV21 else -1
         // Android 5.0.1 bug (https://code.google.com/p/android/issues/detail?id=81984)
         // YUV_420_888 does not contain all U and V data
//         if ((Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP) && (hasNV21))
//            imageFormat = ImageFormat.NV21;
         if (! hasYUV)
         {
            Log.w(TAG, "Camera $id does not support YUV output")
            return false
         }
//         val cameraRotation = characteristics.get(CameraCharacteristics.SENSOR_ORIENTATION)
//         val rotation = (activity.getSystemService(Context.WINDOW_SERVICE) as WindowManager).defaultDisplay.rotation
//         Log.i(TAG, "Camera rotation $cameraRotation Device rotation $rotation")
      }
      catch (e: java.lang.Exception)
      {
         Log.e(TAG, "Error initializing camera $id")
         return false
      }
      return true
   }

   abstract fun startPreview(context: Context, size: Size, callback: CameraPreviewable): Boolean

   abstract fun getHandler(): Handler?

   fun stopPreview() { stopping = true }

   fun initialize(queueSize: Int, isRearFacing: Boolean) : Boolean
   {
      this.queueSize = queueSize
      return addCamera(id, queueSize, isRearFacing)
   }

   open fun stopCamera()
   //-------------------
   {
      if (captureSession != null)
      {
         captureSession?.abortCaptures()
         SystemClock.sleep(30)
         try { captureSession?.close() } catch (ee: java.lang.Exception) {}
         captureSession = null
      }
      if (cameraDevice != null)
      {
         try { cameraDevice?.close() } catch (ee: java.lang.Exception) {}
         cameraDevice = null
      }
      isConfigured.set(false)
   }


   class CameraHandler(looper: Looper) : Handler(looper)
   //===================================================
   {
      override fun handleMessage(msg: Message) = super.handleMessage(msg)
   }

   class CameraLooperThread : Thread()
   //=================================
   {
      var handler: Handler? = null
      var looper: Looper? = null

      override fun run()
      //----------------
      {
         Looper.prepare()
         handler = CameraHandler(Looper.myLooper()!!)
         looper = Looper.myLooper()
         Looper.loop()
      }
   }

   protected fun startBackgroundThread(): Handler
   //-----------------------------------
   {
      val cameraThread = HandlerThread("Camera-$id", Process.THREAD_PRIORITY_FOREGROUND)
      cameraThread.start()
      Log.i(TAG, "Started thread ${cameraThread.id} ${cameraThread.threadId} for camera $cameraId")
      return Handler(cameraThread.looper)
   }

   protected fun stopBackgroundThread(cameraHandler: Handler?)
   //--------------------------------------------------------
   {
      if (cameraHandler == null) return
      val cameraThread = cameraHandler.looper.thread
      cameraHandler.looper.quitSafely()
      cameraThread.interrupt()
      try
      {
         cameraThread.join()
      }
      catch (e: InterruptedException)
      {
         Log.e(TAG, e.toString())
      }
   }

   @Suppress("DEPRECATION")
   companion object
   {
      private val TAG = HardwareCamera::class.java.simpleName

      init
      {
         try
         {
            System.loadLibrary("MAR")
            Log.i("MAR", "Loaded libMAR.so (HardwareCamera)")
         }
         catch (e: Exception)
         {
            Log.e("HardwareCamera", "Exception loading libMAR.so (HardwareCamera)", e)
         }
      }

      fun imageFormatToString(imageFormat: Int): String?
      {
         when (imageFormat)
         {
            ImageFormat.YUV_420_888 -> return "YUV_420_888"
            ImageFormat.NV21 -> return "NV21"
            PixelFormat.RGBA_8888 -> return "RGBA_8888"
            PixelFormat.RGBX_8888 -> return "RGBX_8888"
            ImageFormat.FLEX_RGBA_8888 -> return "FLEX_RGBA_8888"
            ImageFormat.FLEX_RGB_888 -> return "FLEX_RGB_888"
            ImageFormat.RGB_565 -> return "RGB_565"
            ImageFormat.RAW_SENSOR -> return "RAW_SENSOR"
            ImageFormat.DEPTH16 -> return "DEPTH16"
            ImageFormat.DEPTH_POINT_CLOUD -> return "DEPTH_POINT_CLOUD"
            ImageFormat.JPEG -> return "JPEG"
            ImageFormat.NV16 -> return "NV16"
            ImageFormat.PRIVATE -> return "PRIVATE"
            ImageFormat.RAW10 -> return "RAW10"
            ImageFormat.RAW12 -> return "RAW12"
            ImageFormat.UNKNOWN -> return "UNKNOWN"
            ImageFormat.YUV_422_888 -> return "YUV_422_888"
            ImageFormat.YUV_444_888 -> return "YUV_444_888"
            ImageFormat.YUY2 -> return "YUY2"
            ImageFormat.YV12 -> return "YV12"
            PixelFormat.A_8 -> return "A_8"
            PixelFormat.LA_88 -> return "LA_88"
            PixelFormat.L_8 -> return "L_8"
            PixelFormat.OPAQUE -> return "OPAQUE"
            PixelFormat.RGBA_4444 -> return "RGBA_4444"
            PixelFormat.RGBA_5551 -> return "RGBA_5551"
            PixelFormat.RGB_332 -> return "RGB_332"
            PixelFormat.RGB_888 -> return "RGB_888"
            PixelFormat.TRANSLUCENT -> return "TRANSLUCENT"
            PixelFormat.TRANSPARENT -> return "TRANSPARENT"
            else -> return "UNKNOWN"
         }
      }

      fun cameraErrorMessage(error: Int) : String
      //-----------------------------------------
      {
         return when (error)
         {
            CameraDevice.StateCallback.ERROR_CAMERA_DISABLED ->
               "The camera device could not be opened due to a device policy (sic)."
            CameraDevice.StateCallback.ERROR_CAMERA_IN_USE ->
               "The camera device is in use already."
            CameraDevice.StateCallback.ERROR_CAMERA_SERVICE ->
               "The camera service has encountered a fatal error."
            CameraDevice.StateCallback.ERROR_CAMERA_DEVICE ->
               "The camera device has encountered a fatal error."
            CameraDevice.StateCallback.ERROR_MAX_CAMERAS_IN_USE ->
               "The camera device could not be opened because there are too many other open camera devices"
            else -> "Unknown error"
         }
      }

      fun isRearFacing(context: Context,id: String) : Boolean
      //-----------------------------------------------------
      {
         try
         {
            val manager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager
            val characteristics = manager.getCameraCharacteristics(id)
            return characteristics.get(CameraCharacteristics.LENS_FACING) == CameraCharacteristics.LENS_FACING_BACK
         }
         catch (e: java.lang.Exception)
         {
            Log.e(TAG, "camera $id", e)
            return true
         }
      }

  /*
   inner class CameraCaptureCallback : CameraCaptureSession.CaptureCallback()
   {
      override fun onCaptureCompleted(session: CameraCaptureSession,
                                      request: CaptureRequest,
                                      result: TotalCaptureResult)
      {
         super.onCaptureCompleted(session, request, result)
         val results = result.partialResults
         for (r in results)
            Log.i(TAG, r.toString())

      }
   }
   */
   }

   protected abstract fun createCameraPreviewSession(context: Context)

    protected inner class CameraCallback(val context: Context): CameraDevice.StateCallback()
   //========================================================================
   {
      override fun onOpened(cameraDevice: CameraDevice)
      //-------------------------------------------------------
      {
         this@HardwareCamera.cameraDevice = cameraDevice
         openCloseLock.release()
         createCameraPreviewSession(context)
      }

      override fun onDisconnected(cameraDevice: CameraDevice)
      //-------------------------------------------------------------
      {
         cameraDevice.close()
         this@HardwareCamera.cameraDevice = null
         openCloseLock.release()
         Log.w(TAG, "Camera disconnected: ${cameraDevice.id}")
      }

      override fun onError(cameraDevice: CameraDevice, error: Int)
      //-------------------------------------------------------
      {
         cameraDevice.close()
         this@HardwareCamera.cameraDevice = null
         openCloseLock.release()
         Log.e(TAG, "Camera error: ${cameraDevice.id} $error - ${cameraErrorMessage(error)}")
      }
   }
}
