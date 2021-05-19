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


class StandardCamera(context: Context, id: String, colorFormat: ColorFormats,
                     renderscript: RenderScript?, isMultiCamera: Boolean) :
   HardwareCamera(context, id, colorFormat, isMultiCamera, renderscript)
//==============================================================================
{
   companion object
   {
      private val TAG = StandardCamera::class.java.simpleName
   }

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
         {
            manager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager?
         }
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
         if (! openCloseLock.tryAcquire(3000, TimeUnit.MILLISECONDS))
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
         val previewRequestBuilder = cameraDevice?.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW)
         if (previewRequestBuilder == null)
         {
            Log.e(TAG, "Error creating preview request builder for camera $id")
            previewCallback?.onPreviewResult(id, false, "Error creating preview request builder")
            isConfigured.set(false)
            return
         }
         val inputFormat: Int = when
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

         val previewSurface: Surface? = previewFrameHandler?.getSurfaces()?.get(0)
         if (previewSurface == null)
         {
            Log.e(TAG, "ERROR: Camera preview handler does not have a valid surface for camera $id.")
            previewCallback?.onPreviewResult(id, false,
               "ERROR: Camera preview handler does not have a valid surface for camera $id.")
            isConfigured.set(false)
            return
         }
         val cameraStatusCallback = CameraStatusCallback(previewRequestBuilder, previewSurface)
//         assert( threadHandler != null && threadHandler?.looper != null)
         if (previewFrameHandler != null)
            cameraDevice?.createCaptureSession(previewFrameHandler.getSurfaces(), cameraStatusCallback,
                                               threadHandler)
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
         if (E.cause != null)
            Log.e(TAG, "Caused by ${E.cause}", E.cause)
         isConfigured.set(false)
      }
      catch (E: Throwable)
      {
         Log.e(TAG, "ERROR: Creating camera preview session for camera $id $E.", E)
         previewCallback?.onPreviewResult(id, false, "Error creating camera preview session for camera $id")
         isConfigured.set(false)
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

   override fun stopCamera()
   //-----------------------
   {
      stopBackgroundThread(threadHandler)
      super.stopCamera()
   }

   inner class CameraStatusCallback(val previewRequestBuilder: CaptureRequest.Builder,
                                    val previewSurface: Surface) : CameraCaptureSession.StateCallback()
   //=====================================================================
   {
      override fun onConfigured(session: CameraCaptureSession)
      //-----------------------------------------------------------------
      {
         captureSession = session
         try
         {
            previewRequestBuilder.set(CaptureRequest.CONTROL_CAPTURE_INTENT, CaptureRequest.CONTROL_CAPTURE_INTENT_PREVIEW)
//                  previewRequestBuilder.set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_OFF)
            previewRequestBuilder.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_OFF)
            previewRequestBuilder.set(CaptureRequest.CONTROL_AWB_MODE, CaptureRequest.CONTROL_AWB_MODE_OFF)
//                  previewRequestBuilder.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE_OFF)
//                  previewRequestBuilder.set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_ON_AUTO_FLASH)
            previewRequestBuilder.set(CaptureRequest.LENS_FOCUS_DISTANCE, 0.0f)
//                  previewRequestBuilder.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, fps[fps.length - 1]);
            var characteristics = manager!!.getCameraCharacteristics(id)
            val fpsRanges: Array<out Range<Int>>? =
               characteristics.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES)
            var FPS: Range<Int>? = null
            var maxFPS: Range<Int>? = null
            var FPS30: List<Range<Int>?>? = null
            if (fpsRanges != null)
            {
               maxFPS = fpsRanges.maxByOrNull { r -> r.upper + r.lower }
               FPS30 = fpsRanges.filter { r -> r.upper >= 30 || r.lower >= 30 }
                  .sortedByDescending { r -> r.lower + r.upper }
            }
            else
            {
               FPS = Range<Int>(30,30)
               maxFPS = FPS
               FPS30 = listOf(FPS)
            }
            previewRequestBuilder.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, maxFPS)
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
      //---------------------------------------------------------
      {
         Log.e(TAG, "ERROR: createCameraPreviewSession failed for camera $id")
         //try { session.abortCaptures() } catch (_e: Throwable) { Log.e(TAG, "Error aborting capture for camera $id (${_e.message})") }
         isConfigured.set(false)
         previewCallback?.onPreviewResult(id, false,
            "Camera configuration failed for camera $id (onConfigureFailed)")
         openCloseLock.release()
      }
   }
}
