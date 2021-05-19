package no.pack.drill.ararch.mar

import android.content.Context
import android.content.res.AssetManager
import android.graphics.ImageFormat
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraManager
import android.os.Build
import android.os.SystemClock
import android.renderscript.RenderScript
import android.util.Log
import android.util.Range
import android.util.Size
import android.view.Surface
import java.nio.ByteBuffer
import java.util.concurrent.ConcurrentHashMap


object MAR
//=======
{
   init
   {
      try
      {
         System.loadLibrary("MAR")
         Log.i("MAR", "Loaded libMAR.so (MAR)")
      }
      catch (e: Exception)
      {
         Log.e("MAR", "Exception loading libMAR.so (MAR)", e)
      }
   }

   external fun initialize(mainPackageName: String, assman: AssetManager)
   fun addSensor(sensor: Int): Boolean { return addSensors(IntArray(1) { sensor }) }
   external fun addSensors(sensors: IntArray): Boolean
   external fun allocateBuffer(size: Int): ByteBuffer
   external fun setSurface(surface: Surface?, width: Int, height: Int): Boolean
   external fun setDeviceRotation(rotation: Int)
   external fun startMAR(rendererType: Int, aprilTagsOnOff: Boolean, faceRecogOnOff: Boolean): Boolean
   external fun stopMAR()
   external fun getStats(): String

   private var cameras: MutableMap<String, HardwareCamera> = HashMap()
   private var cameraThreads: MutableMap<String, Thread> = HashMap()
   private var isPreviewingReady: MutableMap<String, Boolean> = ConcurrentHashMap()
   private var isPreviewing: Boolean = false

   const val MAX_RENDERSCRIPTS: Int  = 2
   public const val None : String = "None"
   public const val SIMPLE_VULKAN_RENDERER: Int = 0
   public const val OPENGL_BULB_RENDERER: Int = 1
   public const val VULKAN_BULB_RENDERER: Int = 2

   private val TAG = MAR::class.java.simpleName
   private val COLOR_FORMAT: ColorFormats = ColorFormats.RGBA

   fun cameras(context: Context, listIds: MutableList<String>, listSizes: MutableList<Array<Size>>,
               listIsBackward: MutableList<Boolean>? =null, isFacingBack: Boolean = true,
               isFacingFront: Boolean = false, isHighSpeed :Boolean =false):
         Boolean
   //----------------------------------------------------------------------------------------------------------------
   {
      listIds.clear()
      listIsBackward?.clear()
      listSizes.clear()
      val manager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager
      try
      {
         for (id in manager.cameraIdList)
         {
            var hasHighSpeed = false
            if (isHighSpeed)
               hasHighSpeed = FastCamera.hasHighSpeed(context, id)
            val characteristics = manager.getCameraCharacteristics(id)
            val facing = characteristics.get(CameraCharacteristics.LENS_FACING)
            val scalarMap = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)
            if (scalarMap == null)
            {
               Log.e(TAG, "Could not obtain SCALER_STREAM_CONFIGURATION_MAP for $id")
               continue
            }
            if ((facing == CameraCharacteristics.LENS_FACING_BACK) && (isFacingBack))
            {
               listIds.add(id)
               listIsBackward?.add(true)
               if ( (isHighSpeed) && (hasHighSpeed) )
               {
                  listSizes.add(scalarMap.highSpeedVideoSizes)
//                  val fps = scalarMap.highSpeedVideoFpsRanges
//                  fps.size
               }
               else
                  listSizes.add(scalarMap.getOutputSizes(ImageFormat.YUV_420_888))
            }
            else if ((facing == CameraCharacteristics.LENS_FACING_FRONT) && (isFacingFront))
            {
               listIds.add(id)
               listIsBackward?.add(false)
               if ( (isHighSpeed) && (hasHighSpeed) )
                  listSizes.add(scalarMap.getHighSpeedVideoSizesFor(Range(60, 60)))
               else
                  listSizes.add(scalarMap.getOutputSizes(ImageFormat.YUV_420_888))
            }
         }
      }
      catch (e: java.lang.Exception)
      {
         Log.e(TAG, "cameras", e)
         return false
      }
      return (listIds.size > 0)
   }

   val renderscripts: MutableList<RenderScript?> = mutableListOf()

   fun initialize(context: Context, cameraIds: List<String?>, queueSize: Int,
                  wantHighSpeed: Boolean, isRenderScript: Boolean): MutableMap<String, Boolean>
   //----------------------------------------------------------------------
   {
      var results: MutableMap<String, Boolean> = mutableMapOf()
      val isMultiCamera = (cameraIds.size > 1)
      var renderscript: RenderScript? = null
      if (cameraIds.isEmpty())
      {
         Log.e(TAG, "MAR::initialize - No cameras");
         return results
      }
      try
      {
         var manager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager?
         if (manager != null)
         {
            var sortedCameras: List<String?>? =
               try
               {
                  cameraIds.sortedWith(Comparator<String?> { first, second ->
                     var id1 = first?.trim() ?: "999998"
                     if (id1 == None)
                        id1 = "999998"
                     else
                        id1 = if (HardwareCamera.isRearFacing(context, id1)) "0$id1" else "1$id1"
                     var id2 = second?.trim() ?: "999999"
                     if (id2 == None)
                        id2 = "999999"
                     else
                        id2 = if (HardwareCamera.isRearFacing(context, id2)) "0$id2" else "1$id2"
                     id1.compareTo(id2)
                  })
               }
               catch (ee: java.lang.Exception)
               {
                  Log.e(TAG, cameraIds.toString(), ee)
                  null
               }
            if (sortedCameras == null)
               sortedCameras = cameraIds
            for (i in 0 until sortedCameras.size)
            {
               var cameraId = sortedCameras[i]
               if ((cameraId == null) || (cameraId == None))
               {
                  if (cameraId == None)
                     results[None] = false
                  continue
               }
               var hasHighSpeed = false
               if (wantHighSpeed)
                  hasHighSpeed = FastCamera.hasHighSpeed(context, cameraId)
               if (cameras.containsKey(cameraId))
               {
                  val cam: HardwareCamera? = cameras[cameraId]
                  cam?.stopPreview()
                  cameras.remove(cameraId)
               }
               var manager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager?
               if (manager != null)
               {
                  try
                  {
                     val characteristics = manager.getCameraCharacteristics(cameraId)
                     var caps =
                        characteristics.get(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES)
                     if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P)
                     {
                        val isLogicalCam =
                           caps?.contains(CameraCharacteristics.REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA)
                               ?: false
                        val physCameras = characteristics.physicalCameraIds
                        if ((isLogicalCam) && (physCameras.isNotEmpty()))
                           Log.w(TAG, "Camera $cameraId is a logical (multi-camera) device (TODO)")
                        // TODO: deal with multicamera (see https://medium.com/androiddevelopers/getting-the-most-from-the-new-multi-camera-api-5155fb3d77d9)
                     }
                  }
                  catch (ee: java.lang.Exception)
                  {
                     Log.e(TAG, "camera $cameraId", ee)
                  }
               }
               var rs: RenderScript? = if (isRenderScript) createRenderscript(context, isMultiCamera) else null
               val camera: HardwareCamera = if ((wantHighSpeed) && (hasHighSpeed))
                     FastCamera(context, cameraId, COLOR_FORMAT, rs, (sortedCameras.size > 1))
                  else
                     StandardCamera(context, cameraId, COLOR_FORMAT, rs, (sortedCameras.size > 1))
               if (camera.open())
               {
                  cameras[cameraId] = camera
                  results[cameraId] = camera.initialize(queueSize, camera.isRearFacing)
               }
               else
                  results[cameraId] = false
            }
         }
      }
      catch (e: java.lang.Exception)
      {
         Log.e(TAG, cameraIds.toString(), e)
      }
      return results
   }

   private fun createRenderscript(context: Context,  isMultiCamera: Boolean): RenderScript?
   //--------------------------------------------------------------------------------------
   {
      var rs: RenderScript? = null
      try
      {
            if (isMultiCamera)
            {
               if (renderscripts.size < MAX_RENDERSCRIPTS)
               {
                  rs = RenderScript.createMultiContext(context, RenderScript.ContextType.NORMAL,
                     RenderScript.CREATE_FLAG_NONE, 28)
                  renderscripts.add(rs)
               }
               else
                  if (renderscripts.size > 0)
                     rs = renderscripts[0]
            }
            else
            {
               if (renderscripts.size > 0)
                  rs = renderscripts[0]
               else
               {
                  rs = RenderScript.create(context)
                  renderscripts.add(rs)
               }
            }
      }
      catch (Ee: java.lang.Exception)
      {
         Log.e(TAG, "Error creating renderscript", Ee)
         if (renderscripts.size > 0)
            rs = renderscripts[0]
      }
      return rs
   }

   private fun startCameras(context: Context, cameraList: MutableList<Triple<String, Size, HardwareCamera>>)
   //----------------------------------------------------------------------------------------------------
   {
      for (ttt in cameraList)
      {
         val cameraInterface = ttt.third
         val camera: String = ttt.first
         val size: Size = ttt.second
         if (isPreviewingReady.containsKey(camera))
            Log.e(TAG, "Camera $cameraInterface already previewing")
         else
            cameraInterface.startPreview(context, size, object: CameraPreviewable
            {
               override fun onPreviewResult(cameraId: String, isOK: Boolean, message: String)
               //-----------------------------------------------------------------------------
               {
                  isPreviewingReady[cameraId] = isOK
                  if (! isOK)
                     stopPreview(mutableListOf(camera))
               }
            })
      }
   }

   fun startPreview(context: Context, cameraSizes: Map<String, Size>, errbuf: StringBuilder?): Boolean
   //-----------------------------------------------------------------------
   {
      for (camera in cameraSizes.keys)
      {
         if (! cameras.containsKey(camera))
         {
            Log.e(TAG, "Camera $camera not initialised")
            return false
         }
      }
      val rearCameras = mutableListOf<Triple<String, Size, HardwareCamera>>()
      val frontCameras = mutableListOf<Triple<String, Size, HardwareCamera>>()
      cameraSizes.forEach { pp ->
         val cameraInterface = cameras[pp.key]
         if (cameraInterface != null)
         {
            val ttt = Triple(pp.key, pp.value, cameraInterface)
            if (cameraInterface.isRearFacing) rearCameras.add(ttt) else frontCameras.add(ttt)
         }
      }
      startCameras(context, rearCameras)
      startCameras(context, frontCameras)

      val configured : MutableSet<String> = mutableSetOf()
      val badCameras : MutableSet<String> = mutableSetOf()
      var configuredOK = 0
      val start = SystemClock.currentThreadTimeMillis()
      while (configured.size < cameraSizes.size)
      {
         for (camera in cameraSizes.keys)
         {
            if ( (! configured.contains(camera)) && (isPreviewingReady.containsKey(camera)) )
            {
               configured.add(camera)
               if (isPreviewingReady[camera]!!)
                  configuredOK++
               else
                  badCameras.add(camera)
            }
         }
         if (configured.size < cameraSizes.size)
         {
            SystemClock.sleep(20)
            if ( (SystemClock.currentThreadTimeMillis() - start) > 5000 )
               break
         }
      }
      isPreviewing = configuredOK >= cameraSizes.size
      if (! isPreviewing)
      {
         var sb : java.lang.StringBuilder
         if (errbuf == null)
            sb = java.lang.StringBuilder("Cameras that failed to preview: ")
         else
         {
            sb = errbuf
            sb.append("Cameras that failed to preview:")
         }
         badCameras.forEach { it -> sb.append(" ").append(it)}
         Log.e(TAG, sb.toString())
         sb.append(" (See Logcat for details.)")
      }
      return isPreviewing
   }

   fun stopPreview(cameraIds: MutableList<String>)
   //----------------------------------------------
   {
      for (camera in cameraIds)
      {
         if (cameras.containsKey(camera))
         {
            val cam = cameras[camera]
            cam!!.stopPreview()
         }
      }
      for (rs in renderscripts)
         try { rs?.destroy() } catch (t: Throwable) {}
   }
}

