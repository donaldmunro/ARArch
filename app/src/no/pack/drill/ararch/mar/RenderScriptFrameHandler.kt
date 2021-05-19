package no.pack.drill.ararch.mar

import android.graphics.PixelFormat
import android.media.ImageReader
import android.os.SystemClock
import android.renderscript.Allocation
import android.renderscript.Element
import android.renderscript.RenderScript
import android.renderscript.Type
import android.util.Log
import android.view.Surface

class RenderScriptFrameHandler(val renderscript: RenderScript, private val hardwareCamera: HardwareCamera,
                               inputFormat: Int, private val colorFormat: ColorFormats,
                               isMultiCamera: Boolean, private val isGrey: Boolean,
                               private val isGreySeparate: Boolean =false) :
   Allocation.OnBufferAvailableListener, SurfaceProvidable
//=====================================================================
{
   private val cameraId: String = hardwareCamera.cameraId
   val cameraWidth: Int = hardwareCamera.width
   val cameraHeight: Int = hardwareCamera.height
   private lateinit var rsYUVConvert: ScriptC_YUV2RGBA
   private lateinit var allocYUVIn: Allocation
   private lateinit var allocRGBAOut: Allocation
   private var allocGrayOut: Allocation? = null
   private var isStopInit: Boolean = false
   private var rgbaSize: Int =-1
   private var greySize: Int = -1
//   private val FPSInterval: Long = (1000000000L / hardwareCamera.FPS)
   private lateinit var previewSurface: Surface
   private lateinit var dummyImgReader: ImageReader
   private var isGood: Boolean = false
   val good: Boolean
      get() { return isGood }

   companion object
   {
      private val TAG = RenderScriptFrameHandler::class.java.simpleName
      private val DEBUG_SAVE_FRAME = false
      private val DEBUG_SAVE_RAW_FRAME = false
      private val DEBUG_SAVE_FRAME_MAX = 200
      private var DEBUG_FRAME_SEQ = 1
      private var RAW_DEBUG_FRAME_SEQ = 1
   }

   init
   {
      greySize = if (isGrey) hardwareCamera.width * hardwareCamera.height else 0
      rgbaSize = hardwareCamera.width * hardwareCamera.height * 4
      try
      {
         val yuvTypeBuilder = Type.Builder(renderscript, Element.YUV(renderscript))
         yuvTypeBuilder.setX(hardwareCamera.width).setY(hardwareCamera.height).setYuvFormat(inputFormat)
         allocYUVIn = Allocation.createTyped(renderscript, yuvTypeBuilder.create(),
                                             Allocation.USAGE_IO_INPUT or Allocation.USAGE_SCRIPT)
         previewSurface = allocYUVIn.surface

         val rgbTypeBuilder = Type.Builder(renderscript, Element.RGBA_8888(renderscript))
         rgbTypeBuilder.setX(hardwareCamera.width).setY(hardwareCamera.height)
         allocRGBAOut = Allocation.createTyped(renderscript, rgbTypeBuilder.create(),
                                               Allocation.USAGE_IO_OUTPUT or Allocation.USAGE_SCRIPT)
         dummyImgReader = ImageReader.newInstance(hardwareCamera.width, hardwareCamera.height,
                                                  PixelFormat.RGBA_8888, 1)
         allocRGBAOut.surface = dummyImgReader.surface
         rsYUVConvert = ScriptC_YUV2RGBA(renderscript)
         rsYUVConvert!!._YUVinput = allocYUVIn
         if (isGrey)
         {
            val greyTypeBuilder = Type.Builder(renderscript, Element.U8(renderscript))
            greyTypeBuilder.setX(hardwareCamera.width).setY(hardwareCamera.height)
            allocGrayOut = Allocation.createTyped(renderscript, greyTypeBuilder.create(),
                                                  Allocation.USAGE_SCRIPT)
            if (! isGreySeparate)
               rsYUVConvert!!._greyOutput = allocGrayOut
         }
         allocYUVIn.setOnBufferAvailableListener(this)
         isGood = true
      }
      catch (e: java.lang.Exception)
      {
         isGood = false
         Log.e(TAG, "Error initializing renderscript frame handler", e)
      }
   }

//   var noFrames: Long = 0
//   private var lastFrameTime: Long = 0

   override fun onBufferAvailable(allocation: Allocation?)
   //--------------------------------------------
   {
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
      {
         Log.w(TAG, "In flight max exceeded (" + hardwareCamera.inFlight() + ")")
//         hardwareCamera.clearQueue(cameraId)
         return;
      }
//      Log.i(TAG, "onBufferAvailable: $cameraId ${allocation.toString()} ${allocYUVIn.toString()}")
      val ts = SystemClock.elapsedRealtimeNanos() // Allocation.timeStamp is 0

      try
      {
         allocYUVIn.ioReceive()
//         Log.i(TAG, "Received renderscript YUV allocation size " + allocYUVIn.bytesSize + " " +
//                    (cameraWidth * cameraHeight * ImageFormat.getBitsPerPixel(ImageFormat.YUV_420_888) / 8))
//         if (DEBUG_SAVE_RAW_FRAME) saveRaw(allocYUVIn, cameraWidth, cameraHeight)
         when (colorFormat)
         {
            ColorFormats.BGRA     ->
            {
               if (isGrey)
               {
                  if (isGreySeparate)
                  {
                     rsYUVConvert.forEach_YUVtoBGRA(allocRGBAOut)
                     rsYUVConvert.forEach_YUVtoGrey(allocGrayOut)
                  }
                  else
                     rsYUVConvert.forEach_YUVtoBGRAGrey(allocRGBAOut)
               }
               else
                  rsYUVConvert.forEach_YUVtoBGRA(allocRGBAOut);
            }
            ColorFormats.RGBA     ->
            {
               if (isGrey)
               {
                  if (isGreySeparate)
                  {
                     rsYUVConvert.forEach_YUVtoRGBA(allocRGBAOut, null)
                     rsYUVConvert.forEach_YUVtoGrey(allocGrayOut, null)
                  }
                  else
                     rsYUVConvert.forEach_YUVtoRGBAGrey(allocRGBAOut, null)
               }
               else
                  rsYUVConvert.forEach_YUVtoRGBA(allocRGBAOut, null)
            }
         }
         val rgbaData : ByteArray
         try
         {
            rgbaData = ByteArray(rgbaSize)
         }
         catch (e: OutOfMemoryError)
         {
            Log.e(TAG, "RenderScriptFrameHandler out of memory for camera id $cameraId  " +
                  " rear facing = ${hardwareCamera.isRearFacing} (${e.message}). " +
                  "Attempting to clear queue")
            hardwareCamera.clearQueue(cameraId)
            return
         }
         allocRGBAOut.copyTo(rgbaData)
         var greyData :ByteArray? = null
         if (isGrey)
         {
            greyData = ByteArray(greySize)
            allocGrayOut?.copyTo(greyData)
         }
////         if (DEBUG_SAVE_FRAME) saveCooked(allocRGBAOut, qitem.rgbaSize)
//         Log.i(TAG, "Enqueue Time Java: thread ${Thread.currentThread().id} for camera $cameraId ${hardwareCamera.isRearFacing}: ${((ts - lastFrameTime)/1000000)}ms")
         hardwareCamera.enqueue(cameraId, colorFormat==ColorFormats.RGBA, ts,
                                rgbaSize, rgbaData, greySize, greyData)
//         lastFrameTime = ts
      }
      catch (e: java.lang.Exception)
      {
         Log.e(TAG, "Error processing incoming renderscript allocation for " +
               "$cameraId ${hardwareCamera.isRearFacing}", e)
      }
   }

   override fun getSurfaces(): MutableList<Surface?>
   {
      return mutableListOf(previewSurface)
   }

/*
   @SuppressLint("SdCardPath")
   private fun saveRaw(a: Allocation?, cameraWidth: Int, cameraHeight: Int)
   {
      try
      {
         val yuvBuffer = ByteArray(a!!.bytesSize)
         yuvBuffer.fill(255.toByte())
         a.copyTo(yuvBuffer)
//         a.copy2DRangeTo(0, 0, cameraWidth, cameraHeight, yuvBuffer)
         BufferedOutputStream(FileOutputStream(File("/sdcard/Pictures/captured-$cameraId-${RAW_DEBUG_FRAME_SEQ.toString()
            .padStart(6, '0')}.yuv420"))).use { out -> out.write(yuvBuffer) }
         if (RAW_DEBUG_FRAME_SEQ++ > 100)
         {
            val seq = RAW_DEBUG_FRAME_SEQ - DEBUG_SAVE_FRAME_MAX
            File("/sdcard/Pictures/captured-{${seq.toString().padStart(6, '0')}.raw").delete()
            File("/sdcard/Pictures/captured-raw-${seq.toString().padStart(6, '0')}.png").delete()
         }
      }
      catch (_e: java.lang.Exception)
      {
         Log.e(TAG, "Writing Raw decoded YUV", _e)
      }
   }

   @SuppressLint("SdCardPath")
   private fun saveCooked(colorAlloc: Allocation, rgbaSize: Int)
   {
      val colorBmp =
         Bitmap.createBitmap(cameraWidth, cameraHeight, Bitmap.Config.ARGB_8888)
      colorAlloc.copyTo(colorBmp)
      try
      {
         val imgpath = "/sdcard/Pictures/captured-$cameraId-${DEBUG_FRAME_SEQ.toString().padStart(6, '0')}.png"
         BufferedOutputStream(FileOutputStream(File(imgpath)))
            .use { out -> colorBmp.compress(Bitmap.CompressFormat.PNG, 0, out)
         }
      }
      catch (_e: Exception)
      {
         Log.e(TAG, "", _e)
      }
      try
      {
         val rgba = ByteArray(rgbaSize)
         colorAlloc.copyTo(rgba)
         BufferedOutputStream(FileOutputStream(File("/sdcard/Pictures/captured-$cameraId-${DEBUG_FRAME_SEQ.toString().padStart(
            6,
            '0')}.rgba"))).
            use { out -> out.write(rgba) }
      }
      catch (_e: Exception)
      {
         Log.e(TAG, "", _e)
      }
      if (DEBUG_FRAME_SEQ++ > 100)
      {
         val seq = DEBUG_FRAME_SEQ - DEBUG_SAVE_FRAME_MAX
         File("/sdcard/Pictures/captured-${seq.toString().padStart(6, '0')}.png").delete()
         File("/sdcard/Pictures/captured-${seq.toString().padStart(6, '0')}.rgba").delete()
      }
   }
    */
}
