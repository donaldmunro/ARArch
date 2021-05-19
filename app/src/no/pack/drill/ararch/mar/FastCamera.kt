package no.pack.drill.ararch.mar

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.ImageFormat
import android.hardware.camera2.*
import android.os.Handler
import android.renderscript.RenderScript
import android.util.Log
import android.util.Range
import android.util.Size
import android.view.Surface
import java.lang.Exception
import java.util.concurrent.TimeUnit

/*
Not working at the moment as there does not appear to be any way to send the frames from
the high speed API to anything other than a Surface.
 */
class FastCamera(context: Context, id: String, colorFormat: ColorFormats,
                 renderscript: RenderScript?, isMultiCamera: Boolean) :
   HardwareCamera(context, id, colorFormat, isMultiCamera, renderscript)
//==============================================================================
{
   companion object
   {
      private val TAG = FastCamera::class.java.simpleName

      public fun hasHighSpeed(context: Context, id: String) : Boolean
      //-------------------------------------------------------------
      {
         val manager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager
         val characteristics = manager.getCameraCharacteristics(id)
         var caps = characteristics.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES);
         if (caps == null) return false
         return caps.filter { it -> it == CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_CONSTRAINED_HIGH_SPEED_VIDEO }
            .isNotEmpty()
      }
   }

   private var fastFPS: Range<Int>? = null
   private var rgbaSize: Int =-1
   private var greySize: Int = -1
   private var rsFrameHandler: RenderScriptFrameHandler? = null
   private var cpuFrameHandler: CPUFrameHandler? = null
   private var threadHandler: Handler? = null
   override fun getHandler(): Handler? = threadHandler

   @SuppressLint("MissingPermission")
   override fun startPreview(context: Context, size: Size, callback: CameraPreviewable): Boolean
   //-------------------------------------------------------------------------
   {
      if (manager == null)
      {
         try
         { manager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager? }
         catch (e: Exception)
         {
            return false
         }
      }
      cameraWidth = size.width
      cameraHeight = size.height
      previewCallback = callback
      greySize = cameraWidth * cameraHeight
      rgbaSize = greySize * 4
      setPreviewSize(id, cameraWidth, cameraHeight)
      isConfigured.set(false)
      var isLock = false
      stopping = false
      try
      {
         if (! openCloseLock.tryAcquire(2500, TimeUnit.MILLISECONDS))
         {
            Log.e(TAG,"Time out waiting to lock camera $id for opening.")
            return false
         }
         isLock = true
         stateCallback = CameraCallback(context)
         stopBackgroundThread(threadHandler)
         threadHandler = startBackgroundThread()
         if (threadHandler == null)
         {
            Log.e(TAG, "Could not start a handler thread for $cameraId")
            return false
         }
         Log.i(TAG, "Started handler ${threadHandler!!.looper.thread.id} for camera $id")
         manager!!.openCamera(id, stateCallback as CameraCallback, threadHandler)
      }
      catch (e: java.lang.Exception)
      {
         if (isLock)
            openCloseLock.release()
         stopBackgroundThread(threadHandler)
         return false
      }
      return true
   }

//   var tex: SurfaceTexture = SurfaceTexture(999)
//   var texSurface = Surface(tex)
//   var surfaceView: SurfaceView? = null
//   val preview = SurfaceTexture( 1)
//   val previewSurface = Surface(preview)

   override fun createCameraPreviewSession(context: Context)
   //------------------------------------------------------
   {
      if (cameraDevice == null)
      {
         Log.e(TAG, "ERROR: Camera device not open for camera $id")
         return
      }
      var previewFrameHandler: SurfaceProvidable?
      try
      {
         val characteristics = manager!!.getCameraCharacteristics(cameraId)
         val map = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)
         var standardFPS:  Range<Int>? = null
         if (map != null)
         {
            var availFPS = characteristics.get<Array<Range<Int>>>(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES)
            if (availFPS != null)
            {
               standardFPS = availFPS.filter { it -> ( (it.lower == 60) or (it.upper == 60) ) }
                  .ifEmpty { listOf(null) }.first()
            }
            availFPS = map.getHighSpeedVideoFpsRangesFor(Size(cameraWidth, cameraHeight))
            fastFPS = availFPS.filter { it -> ( (it.lower >= 60) or (it.upper >= 60) ) }.
            ifEmpty { listOf(availFPS[availFPS.lastIndex/2]) }.first()
         }
         else
            fastFPS = Range(60, 60)
         val previewRequestBuilder = cameraDevice?.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW)
         if (previewRequestBuilder == null)
         {
            Log.e(TAG, "Error creating preview request builder for camera $id")
            previewCallback?.onPreviewResult(id, false, "Error creating preview request builder")
            isConfigured.set(false)
            return
         }
         var inputFormat: Int = when
         {

            hasYUV -> ImageFormat.YUV_420_888
            hasNV21 -> ImageFormat.NV21
            else -> ImageFormat.UNKNOWN
         }
         Log.i(TAG, "Using Format ${imageFormatToString(inputFormat)}")
         previewFrameHandler = null
         if (renderscript != null)
         {
            rsFrameHandler = RenderScriptFrameHandler(renderscript, this, inputFormat, colorFormat,
                                                      isMultiCamera, false)
            Log.i(TAG,"Renderscript frame handler $rsFrameHandler for $cameraId")
            if ( (rsFrameHandler != null) && (rsFrameHandler!!.good) )
               previewFrameHandler = rsFrameHandler
         }
         if (previewFrameHandler == null)
         {
            cpuFrameHandler = CPUFrameHandler(context, this, inputFormat, colorFormat, false)
            if (cpuFrameHandler?.good!!)
            {
               if (renderscript != null)
                  Log.e(TAG, "Error initializing Renderscript frame handler. Reverting to CPU handler.")
               previewFrameHandler = cpuFrameHandler
            }
            else
            {
               Log.e(TAG, "ERROR: Could not create a frame handler for camera $id.")
               previewCallback?.onPreviewResult(id, false,
                  "ERROR: Could not create a frame handler for camera $id.")
               isConfigured.set(false)
               return
            }
         }
         if (previewFrameHandler != null)
         {
            val callback = CameraStatusCallback(previewRequestBuilder,
                                                previewFrameHandler.getSurfaces()[0])
            cameraDevice?.createConstrainedHighSpeedCaptureSession(previewFrameHandler.getSurfaces(),
                                                                   callback, threadHandler)
//            surfaceView = SurfaceView(context)
//            surfaceView!!.holder.setFixedSize(cameraWidth, cameraHeight)
//            surfaceView!!.holder.setFormat(ImageFormat.YUV_420_888)
//            preview.setDefaultBufferSize(cameraWidth, cameraHeight)
//            val sessionOutputs = mutableListOf(surfaceView!!.holder.surface)
//            tex.setDefaultBufferSize(cameraWidth, cameraHeight)
//            val sessionOutputs= mutableListOf(texSurface)
//            cameraDevice?.createConstrainedHighSpeedCaptureSession(sessionOutputs, callback, threadHandler)
         }
         else
         {
            Log.e(TAG, "Error initializing frame preview handler for camera $id")
            previewCallback?.onPreviewResult(id, false, "Error initializing frame preview handler for camera $id")
            isConfigured.set(false)
         }
      }
      catch (E: CameraAccessException)
      {
         Log.e(TAG, "ERROR: Creating camera preview session for camera $id ${E.reason}-${cameraErrorMessage(E.reason)}", E)
         previewCallback?.onPreviewResult(id, false, "Error creating camera preview session for camera $id ${E.reason}-${cameraErrorMessage(E.reason)}")
         isConfigured.set(false)
      }
      catch (E: Throwable)
      {
         Log.e(TAG, "ERROR: Creating camera preview session for camera $id", E)
         previewCallback?.onPreviewResult(id, false, "Error creating camera preview session for camera $id")
         isConfigured.set(false)
      }
   }

   inner class CameraStatusCallback(val previewRequestBuilder: CaptureRequest.Builder,
                                    val previewSurface: Surface?) : CameraCaptureSession.StateCallback()
   //=====================================================================
   {
      override fun onConfigured(session: CameraCaptureSession)
      //------------------------------------------------------
      {
         captureSession = session
         try
         {
            previewRequestBuilder.set(CaptureRequest.CONTROL_CAPTURE_INTENT, CaptureRequest.CONTROL_CAPTURE_INTENT_PREVIEW)
//            previewRequestBuilder.set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_OFF)
            previewRequestBuilder.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_OFF)
            previewRequestBuilder.set(CaptureRequest.CONTROL_AWB_MODE, CaptureRequest.CONTROL_AWB_MODE_OFF)
//            previewRequestBuilder.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, Range<Int>(30,120));
            previewRequestBuilder.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, Range<Int>(60,60));
//            previewRequestBuilder.set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_ON_AUTO_FLASH)
            previewRequestBuilder.set(CaptureRequest.LENS_FOCUS_DISTANCE, 0.0f)
            if (previewSurface == null)
            {
               Log.e(TAG, "ERROR: Camera preview handler does not have a valid surface for camera $id.")
               previewCallback?.onPreviewResult(id, false,
                  "ERROR: Camera preview handler does not have a valid surface for camera $id.")
               isConfigured.set(false)
               return
            }
            else
               previewRequestBuilder.addTarget(previewSurface)
//                  val callback: CameraCaptureSession.CaptureCallback? = if (DEBUG_SAVE_FRAME) CameraCaptureCallback () else null
                  Log.i(TAG, "Camera status callback thread  ${Thread.currentThread().id}")
            captureId = session.setRepeatingRequest(previewRequestBuilder.build(), null, threadHandler)
            Log.i(TAG, "CameraPreviewSession for camera $id has started")
            previewCallback?.onPreviewResult(id, true, "OK")
            isConfigured.set(true)
         }
         catch (e: Throwable)
         {
            Log.e(TAG, "ERROR: createCaptureSession for camera $id failed", e)
            previewCallback?.onPreviewResult(id, false, "createCaptureSession failed")
            isConfigured.set(false)
         }
         finally
         {
            openCloseLock.release()
         }

      }

      override fun onConfigureFailed(session: CameraCaptureSession)
      //-----------------------------------------------------------
      {
         Log.e(TAG, "ERROR: createCameraPreviewSession failed in high speed mode for camera $id")
         //try { session.abortCaptures() } catch (_e: Throwable) { Log.e(TAG, "Error aborting capture for camera $id (${_e.message})") }
         isConfigured.set(false)
         previewCallback?.onPreviewResult(id, false,
        "Camera configuration failed in high speed mode for camera $id (onConfigureFailed)")
         openCloseLock.release()
      }
   }

   override fun stopCamera()
   //-----------------------
   {
      stopBackgroundThread(threadHandler)
      super.stopCamera()
   }
}
