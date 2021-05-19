package no.pack.drill.ararch.mar

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.ImageFormat
import android.media.Image
import android.media.ImageReader
import android.os.SystemClock
import android.util.Log
import android.view.Surface
import java.io.BufferedOutputStream
import java.io.File
import java.io.FileOutputStream
import java.nio.ByteBuffer

class CPUFrameHandler(val context: Context, private val hardwareCamera: HardwareCamera, private val inputFormat: Int,
                      private val colorFormat: ColorFormats, private val isGrey: Boolean = false):
   ImageReader.OnImageAvailableListener, SurfaceProvidable
{
   private val cameraId: String = hardwareCamera.cameraId
   val cameraWidth: Int = hardwareCamera.width
   val cameraHeight: Int = hardwareCamera.height
   private var lastFrameTime: Long = 0
   private var isStopInit: Boolean = false
   private lateinit var previewSurface: Surface
   private var imgReader: ImageReader
   private var isGood: Boolean = false
   val good: Boolean
      get() { return isGood }

   init
   {
      imgReader = ImageReader.newInstance(cameraWidth, cameraHeight, inputFormat, 2)
      if (imgReader == null)
         isGood = false
      else
      {
         previewSurface = imgReader.surface
         imgReader.setOnImageAvailableListener(this, hardwareCamera.getHandler())
         isGood = true
      }
   }

   private external fun CPUConvertYUV(YUV: ByteArray, w: Int, h: Int, isRGBA: Boolean,
                                      rgbaData: ByteArray, greyData: ByteArray?): Boolean
   private external fun CPUConvertNV21(Y: ByteBuffer?, U: ByteBuffer?, V: ByteBuffer?, w: Int, h: Int,
                                       isRGBA: Boolean, rgbaData: ByteArray, greyData: ByteArray?): Boolean

   override fun onImageAvailable(reader: ImageReader?)
   //------------------------------------------------
   {
      var image = imgReader.acquireLatestImage()
      if (image == null)
         return
      if (hardwareCamera.isStopping)
      {
         if (! isStopInit)
         {
            try { hardwareCamera.stopCamera() } catch (ee: java.lang.Exception) {}
            isStopInit = true
         }
         return
      }
      if (hardwareCamera.inFlight() > hardwareCamera.queueSize)
         return;
      val ts = SystemClock.elapsedRealtimeNanos()
      try
      {
         val planes = image.planes
         val w = image.width
         val h = image.height
   //         val chromaPixelStride = planes[1].pixelStride
         if (inputFormat == ImageFormat.YUV_420_888) // && (chromaPixelStride != 2) )
         {
            val YUV = ByteArray(w * (h + h / 2))
            val Y = planes[0].buffer
            val U = planes[1].buffer
            val V = planes[2].buffer

            Y.get(YUV, 0, w * h)

            val chromaRowStride = planes[1].rowStride
            val chromaRowPadding = chromaRowStride - w / 2

            var offset = w * h
            if (chromaRowPadding == 0)
            {
               U.get(YUV, offset, w * h / 4)
               offset += (w*h)/4
               V.get(YUV, offset, w * h / 4)
            }
            else
            {
               for (i in 0 until h / 2)
               {
                  U.get(YUV, offset, w / 2)
                  offset += w / 2
                  if (i < h / 2 - 1)
                     U.position(U.position() + chromaRowPadding)

               }
               for (i in 0 until h / 2)
               {
                  V.get(YUV, offset, w / 2)
                  offset += w / 2
                  if (i < h / 2 - 1)
                     V.position(V.position() + chromaRowPadding)
               }
            }

            val greySize = if (isGrey) w*h else 0
            val rgbaSize = w*h*4
            val rgbaData : ByteArray
            val greyData  : ByteArray?
            try
            {
               rgbaData = ByteArray(rgbaSize)
               greyData = if (isGrey) ByteArray(greySize) else null
            }
            catch (e: OutOfMemoryError)
            {
               Log.e(TAG, "CPUFrameHandler out of memory for camera id $cameraId  " +
                     " rear facing = ${hardwareCamera.isRearFacing} (${e.message}). " +
                     "Attempting to clear queue")
               hardwareCamera.clearQueue(cameraId)
               return
            }

//            Log.i(TAG, "Enqueue Time Java CPU: thread ${Thread.currentThread().id} for camera $cameraId ${hardwareCamera.isRearFacing}: ${((ts - lastFrameTime)/1000000)}ms")
            if (! hardwareCamera.enqueueYUV(cameraId, YUV, w, h, colorFormat==ColorFormats.RGBA,
                                            ts, rgbaSize, rgbaData, greySize, greyData))
               Log.e(TAG, "Error enqueueing frame for camera $cameraId (rear facing ${hardwareCamera.isRearFacing})")

   //            CPUConvertYUV(YUV, w, h, colorFormat==ColorFormats.RGBA, rgbaData, greyData)
   //           if (DEBUG_SAVE_FRAME) saveCooked(cameraId, rgbaData, cameraWidth, cameraHeight)
   //            hardwareCamera.enqueue(cameraId, colorFormat==ColorFormats.RGBA, ts,
   //                                   rgbaSize, rgbaData, greySize, greyData)

   //         val yuv_mat = Mat(h + h / 2, w, CvType.CV_8UC1)
   //         yuv_mat.put(0, 0, yuv_bytes)
   //         Imgproc.cvtColor(yuv_mat, mRgba, Imgproc.COLOR_YUV2RGBA_I420, 4)
         }
         else if (inputFormat == ImageFormat.NV21)
         { // Chroma channels are interleaved
            val Y = planes[0].buffer
            val UV1 = planes[1].buffer
            val UV2 = planes[2].buffer
            val greySize = if (isGrey) w*h else 0
            val rgbaSize = greySize*4
            var rgbaData: ByteArray? = null
            var greyData: ByteArray? = null
            try
            {
               rgbaData = ByteArray(rgbaSize)
               greyData = if (isGrey) ByteArray(greySize) else null
            }
            catch (e: OutOfMemoryError)
            {
               Log.e(TAG, "CPUFrameHandler out of memory for camera id $cameraId  " +
                     " rear facing = ${hardwareCamera.isRearFacing} (${e.message}). " +
                     "Attempting to clear queue")
               hardwareCamera.clearQueue(cameraId)
               return
            }
            image.close()
            image = null
            CPUConvertNV21(Y, UV1, UV2, w, h, colorFormat==ColorFormats.RGBA, rgbaData, greyData)
            hardwareCamera.enqueue(cameraId, colorFormat==ColorFormats.RGBA, ts,
                                   rgbaSize, rgbaData, greySize, greyData)

   //         val y_mat = Mat(h, w, CvType.CV_8UC1, y_plane)
   //         val uv_mat1 = Mat(h / 2, w / 2, CvType.CV_8UC2, uv_plane1)
   //         val uv_mat2 = Mat(h / 2, w / 2, CvType.CV_8UC2, uv_plane2)
   //         val addr_diff = uv_mat2.dataAddr() - uv_mat1.dataAddr()
   //         if (addr_diff > 0)
   //         {
   //            assert(addr_diff == 1L)
   //            Imgproc.cvtColorTwoPlane(y_mat, uv_mat1, mRgba, Imgproc.COLOR_YUV2RGBA_NV12)
   //         }
   //         else
   //         {
   //            assert(addr_diff == -1)
   //            Imgproc.cvtColorTwoPlane(y_mat, uv_mat2, mRgba, Imgproc.COLOR_YUV2RGBA_NV21)
   //         }
   //         return mRgba
         }
         else
            Log.e(TAG, "Invalid or mismatched image format $inputFormat in CPUFrameHandler.onImageAvailable")
      }
      catch (th: Throwable)
      {
         Log.e(TAG, "", th);
      }
      finally
      {
         image?.close()
      }
      lastFrameTime = ts
   }

   override fun getSurfaces(): MutableList<Surface?> = mutableListOf(previewSurface)

   companion object
   {
      private val TAG = CPUFrameHandler::class.java.simpleName

      private val DEBUG_SAVE_FRAME = false
      private val DEBUG_SAVE_FRAME_MAX = 200
      private var DEBUG_FRAME_SEQ = 1

      @SuppressLint("SdCardPath")
      fun saveCooked(cameraId: String, rgba: ByteArray, w: Int, h: Int)
      //-------------------------------------
      {
         try
         {
            BufferedOutputStream(FileOutputStream(File("/sdcard/Pictures/captured-$cameraId-${DEBUG_FRAME_SEQ.toString()
               .padStart(6, '0')}.rgba"))).use { out -> out.write(rgba) }
         }
         catch (_e: Exception)
         {
            Log.e(TAG, "", _e)
         }
         var i: Int = 0
         var j: Int = 0
         val colors = IntArray(rgba.size / 4)
         while (i < rgba.size)
         {
            var r: Int = rgba[i++].toInt()
            if (r < 0) r += 256;
            var g: Int = rgba[i++].toInt()
            if (g < 0) g += 256;
            var b: Int = rgba[i++].toInt()
            if (b < 0) b += 256;
            var a: Int = rgba[i++].toInt()
            if (a < 0) a += 256;
            colors[j++] = Color.argb(a, r, g, b);
         }
         val colorBmp = Bitmap.createBitmap(colors, w, h, Bitmap.Config.ARGB_8888)
         try
         {
            val imgpath = "/sdcard/Pictures/captured-$cameraId-${DEBUG_FRAME_SEQ.toString()
               .padStart(6, '0')}.png"
            BufferedOutputStream(FileOutputStream(File(imgpath)))
               .use { out ->
                  colorBmp.compress(Bitmap.CompressFormat.PNG, 0, out)
               }
         }
         catch (_e: Exception)
         {
            Log.e(TAG, "saveCooked", _e)
         }
         if (DEBUG_FRAME_SEQ++ > 100)
         {
            val seq = DEBUG_FRAME_SEQ - DEBUG_SAVE_FRAME_MAX
            File("/sdcard/Pictures/captured-${seq.toString().padStart(6, '0')}.png").delete()
            File("/sdcard/Pictures/captured-${seq.toString().padStart(6, '0')}.rgba").delete()
         }
      }
   }
}
