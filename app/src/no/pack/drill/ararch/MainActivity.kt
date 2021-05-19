package no.pack.drill.ararch

import android.Manifest
import android.app.Dialog
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.content.pm.PackageManager
import android.hardware.Sensor
import android.os.Bundle
import android.os.Environment
import android.util.Size
import android.view.View
import android.view.WindowManager
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Spinner
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDialogFragment
import kotlinx.android.synthetic.main.activity_main.*
import no.pack.drill.ararch.mar.HardwareCamera
import no.pack.drill.ararch.mar.MAR
import no.pack.drill.ararch.mar.SmartSize
import no.pack.drill.ararch.mar.getPreviewValidSizes
import org.json.JSONObject
import org.json.JSONTokener
import java.io.File
import java.lang.Exception

class MainActivity : AppCompatActivity()
//======================================
{
   companion object
   {
      private val TAG = MainActivity::class.java.simpleName
      private const val DEFAULT_QUEUE_SIZE: Int = 8
      const val RENDERER_TYPE = MAR.SIMPLE_VULKAN_RENDERER
//      val RENDERER_TYPE = MAR.OPENGL_BULB_RENDERER
      const val WANT_HIGH_SPEED = false // Not working at the moment as there does not appear to be
                                       // any way to send the frames from the high speed API to
                                       // anything other than a Surface.

//      var dialog : MessageDialog? = null
//
//      public fun message(activity: Activity,  message: String, isExit: Boolean =false,
//                         isYesNo: Boolean =false, noText: String = "No", yesText: String = "Yes")
//      //------------------------------
//      {
//         val messageAlert = AlertDialog.Builder(activity)
//         if (isExit)
//            messageAlert.setTitle("Exit")
//         else
//            messageAlert.setTitle("Message")
//         messageAlert.setMessage(message)
//         if (isYesNo)
//         {
//            isMessageYes = false
//            messageAlert.setNegativeButton(noText)  { _, _ -> isMessageYes = false }
//            messageAlert.setPositiveButton(yesText) { _, _ -> isMessageYes = true }
//         }
//         else if (isExit)
//            messageAlert.setPositiveButton("Ok") { _, _ -> if (isExit) activity.finish() }
//         val dialog = messageAlert.create()
//         try { dialog!!.show() } catch (e: java.lang.Exception) { Log.e(TAG, "message dialog", e)}
//      }
      public fun message(activity: AppCompatActivity,  message: String, isExit: Boolean =false,
                         isYesNo: Boolean =false, noText: String = "No", yesText: String = "Yes",
                         onFinish: MessageDialogCommand? =null)
      //-----------------------------------------------------------------------------------------
      {
         val args = Bundle()
         args.putString("message", message)
         args.putBoolean("isExit", isExit)
         args.putBoolean("isYesNo", isYesNo)
         args.putString("noText", noText)
         args.putString("yesText", yesText)
         val dialog = MessageDialog()
         dialog!!.arguments = args
//         messageActivity = activity
         dialog!!.messageActivity = activity
         dialog!!.completionCommand = onFinish
         val fm = activity.supportFragmentManager
         val ft = fm.beginTransaction()
         val prev = fm.findFragmentByTag("messageDialog")
         if (prev != null)
            ft.remove(prev)
         ft.addToBackStack(null)
         dialog!!.show(ft, "messageDialog")
      }
   }

   abstract class MessageDialogCommand: Runnable
   {
      var isMessageYes = false

      abstract override fun run()
   }

   class MessageDialog : AppCompatDialogFragment()
   //==================================
   {
      var messageActivity:  AppCompatActivity? = null

      var completionCommand: MessageDialogCommand? = null

      override fun onCreateDialog(savedInstanceState: Bundle?): Dialog
      //--------------------------------------------------------------
      {
         super.onCreateDialog(savedInstanceState)
         val args = arguments ?: throw RuntimeException("No arguments for MessageDialog")
         val message = args.getString("message")
         val isExit = args.getBoolean("isExit")
         val isYesNo = args.getBoolean("isYesNo")
         val noText = args.getString("noText")
         val yesText = args.getString("yesText")
         val messageAlert = AlertDialog.Builder(requireContext())
         if (isExit)
            messageAlert.setTitle("Exit")
         else
            messageAlert.setTitle("Message")
         messageAlert.setMessage(message)
         if (isYesNo)
         {
            if (completionCommand != null)
               completionCommand!!.isMessageYes = false
            messageAlert.setNegativeButton(noText)  { _, _ ->
               if (completionCommand != null)
               {
                  completionCommand?.isMessageYes = false
                  messageActivity?.runOnUiThread(completionCommand)
               }
            }
            messageAlert.setPositiveButton(yesText) { _, _ ->
               if (completionCommand != null)
               {
                  completionCommand?.isMessageYes = true
                  messageActivity?.runOnUiThread(completionCommand)
               }
            }
         }
         else if (isExit)
            messageAlert.setPositiveButton("Ok") { _, _ ->
               if (isExit)
                  messageActivity?.finish()
               else if (completionCommand != null)
                  messageActivity?.runOnUiThread(completionCommand)
            }
         else if (completionCommand != null)
         {
            messageAlert.setOnDismissListener(object : DialogInterface.OnDismissListener
            {
               override fun onDismiss(dialog: DialogInterface?)
               {
                  messageActivity?.runOnUiThread(completionCommand)
               }

            })
         }
         return messageAlert.create()
      }
   }

   lateinit var directory: File
   private var isCamerasList = false
   private var backCameras: MutableList<String> = emptyList<String>().toMutableList()
   private var frontCameras: MutableList<String> = emptyList<String>().toMutableList()
   private var backCameraSizes: MutableList<Array<Size>> = emptyList<Array<Size>>().toMutableList()
   private var frontCameraSizes: MutableList<Array<Size>> = emptyList<Array<Size>>().toMutableList()
   private var activeCameras: HashMap<String, Size?> = HashMap()
   private lateinit var camera0Listener: SelectCameraAdapter
   private lateinit var camera1Listener: SelectCameraAdapter
   private lateinit var camera2Listener: SelectCameraAdapter

   override fun onCreate(savedInstanceState: Bundle?)
   //-----------------------------------------------
   {
      super.onCreate(savedInstanceState)
      setContentView(R.layout.activity_main)
      MAR.initialize(packageName, this.assets)
      checkPermissions()
   }

   private fun init()
   //-------------------
   {
      val rotation = (getSystemService(Context.WINDOW_SERVICE) as WindowManager).defaultDisplay.rotation
      MAR.setDeviceRotation(rotation)
      isCamerasList = false
      camera0Listener = SelectCameraAdapter(camera0List, false)
      camera1Listener = SelectCameraAdapter(camera1List, false)
      camera2Listener = SelectCameraAdapter(camera2List, true)
      camera0Spinner.onItemSelectedListener = camera0Listener
      val camera0SizeListener = SelectCameraSizeAdapter()
      camera0List.onItemSelectedListener = camera0SizeListener
      camera1Spinner.onItemSelectedListener = camera1Listener
      val camera1SizeListener = SelectCameraSizeAdapter()
      camera1List.onItemSelectedListener = camera1SizeListener
      camera2Spinner.onItemSelectedListener = camera2Listener
      val camera2SizeListener = SelectCameraSizeAdapter()
      camera2List.onItemSelectedListener = camera2SizeListener

      onUpdateCameras(null)
      isCamerasList = true
      startButton.setOnClickListener {
      //------------------------------
         if (startButton.text == "Busy") return@setOnClickListener
         if (startButton.text == "Stop")
         {
            startButton.text == "Start"
            MAR.stopPreview(activeCameras.keys.toMutableList())
            return@setOnClickListener
         }
         var cid0 : String? = camera0Listener.selectedCamera
         var cid1 : String? = camera1Listener.selectedCamera
         var cid2 : String? = camera2Listener.selectedCamera
         if ( (cid0 == MAR.None) && (cid1 == MAR.None) && (cid2 == MAR.None) )
         {
            message(this,"Please select at least one camera")
            return@setOnClickListener
         }
         if ( (cid0 == cid1) && (cid0 != MAR.None) )
         {
            message(this, "Cannot select same camera for different video streams")
            return@setOnClickListener
         }
         val size0 = camera0SizeListener.selectedSize
         if ( (cid0 != MAR.None) && (size0 == null) )
         {
            message(this, "Size not selected for camera 1")
            return@setOnClickListener
         }
         val size1 :Size? = camera1SizeListener.selectedSize
         if ( (cid1 != MAR.None) && (size1 == null) )
         {
            message(this,"Size not selected for camera 2")
            return@setOnClickListener
         }
         val size2 :Size? = camera2SizeListener.selectedSize
         if ( (cid2 != MAR.None) && (size2 == null) )
         {
            message(this, "Size not selected for camera 3")
            return@setOnClickListener
         }
         val cameraList = listOf(cid0, cid1, cid2)
         var results = MAR.initialize(this, cameraList, DEFAULT_QUEUE_SIZE, WANT_HIGH_SPEED,
                                      isRenderscript.isChecked)
         cid0 = checkResults(cid0, size0, 0, results)
         if (cid0 == null)
            return@setOnClickListener
         cid1 = checkResults(cid1, size1, 1, results)
         if (cid1 == null)
            return@setOnClickListener
         cid2 = checkResults(cid2, size2, 2, results)
         if (cid2 == null)
            return@setOnClickListener
         if ( (cid0 == MAR.None) && (cid1 == MAR.None) && (cid2 == MAR.None) )
            return@setOnClickListener

         startButton.text = "Stop"
         val extras = Bundle()
         extras.putSerializable("cameras", activeCameras)
         extras.putBoolean("aprilTags", aprilTagsOnOff.isChecked)
         extras.putBoolean("faceRecog", faceRecogOnOff.isChecked)
         val intent = Intent(this, ARActivity::class.java).apply { putExtras(extras) }
         startActivityForResult(intent, 1)
      } // startButton.setOnClickListener

      MAR.addSensor(Sensor.TYPE_GRAVITY)
      MAR.addSensor(Sensor.TYPE_ROTATION_VECTOR)
   }

   private fun checkResults(cid: String?, size: Size?, i: Int,
                            results: MutableMap<String, Boolean>): String?
   //----------------------------------------------------------------------
   {
      if (cid == null) return null
      var ret : String? = cid
      if (cid != MAR.None)
      {
         val msg = "No camera size or camera initialization failed for camera ${i+1} ($cid). Continue ?"
         if (!results[cid]!!)
            return MAR.None
         else if (size != null)
            activeCameras.put(cid, size)
         else
            return MAR.None
      }
      return ret
   }


   override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?)
   //-----------------------------------------------------------------------------
   {
      super.onActivityResult(requestCode, resultCode, data)
      when (requestCode)
      {
         1 -> parseBenchmark()
      }
   }

   private fun parseBenchmark()
   //---------------------------
   {
      val statsJson = MAR.getStats()
      message(this, statsJson)
      var stats :JSONObject?
      try { stats = JSONTokener(statsJson).nextValue() as JSONObject } catch (e: Exception) { stats = null }
      if (stats != null)
      {
         var detectStats: JSONObject?
         try { detectStats = stats.getJSONObject("detector") }  catch (e: Exception) { detectStats = null }
         var mean: Double
         if (detectStats != null)
            mean = detectStats.getDouble("mean")
      }
   }

   private fun checkPermissions()
   //----------------------------
   {
      var filePermission = checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)
      var cameraPermission = checkSelfPermission(Manifest.permission.CAMERA)
      var outstandingPermissions = mutableListOf<String>()
      if (filePermission == PackageManager.PERMISSION_GRANTED)
      {
         directory = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS)
         directory = File(directory, "ARArch")
         directory.mkdirs()
      }
      else
         outstandingPermissions.add(Manifest.permission.WRITE_EXTERNAL_STORAGE)
      if (cameraPermission == PackageManager.PERMISSION_GRANTED)
         init()
      else
         outstandingPermissions.add(Manifest.permission.CAMERA)
      if (outstandingPermissions.size > 0)
         requestPermissions(outstandingPermissions.toTypedArray(), 1)
   }

   override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, granted: IntArray)
   //-----------------------------------------------------------------------------------------
   {
      when (requestCode)
      {
         1 ->
         {
            for (i in 0 until permissions.size)
            {
               val permission = permissions[i]
               val result = granted[i]
               when (permission)
               {
                  Manifest.permission.WRITE_EXTERNAL_STORAGE ->
                  {
                     if (result == PackageManager.PERMISSION_GRANTED)
                     {
                        directory = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS)
                        directory.mkdirs()
                     }
                     else
                     {
                        val dir = getExternalFilesDir(null)
                        if (dir != null)
                        {
                           directory = dir
                           message(this, "Saving images not available in Documents. Saving to $dir")
                        }
                        else
                           message(this, "Saving images not available as write permission denied")
                     }
                  }
                  Manifest.permission.CAMERA ->
                  {
                     if (result == PackageManager.PERMISSION_DENIED)
                        message(this, "Cannot continue without camera permissions", true)
//                     else
                     init()
                  }
               }
            }
         }
      }
   }

   private val cameraSizes: MutableMap<String, MutableSet<Size>> = mutableMapOf()

   private fun addSizes(cameras: MutableList<String>, cameraSizes: MutableList<Array<Size>>)
   //--------------------------------------------------------------
   {
      for (i in 0 until cameras.size)
      {
         val cid = cameras[i]
         val validSizes = if (cameraSizes.size <= i) emptyArray<SmartSize>() else getPreviewValidSizes(this, null, cameraSizes[i])
//         val validSizes = getPreviewValidSizes(this, null, cameraSizes[i])
         for (vsz in validSizes)
            this.cameraSizes.getOrPut(cid) { mutableSetOf() }.add(vsz.size)
      }
   }

   private fun onUpdateCameras(vw : View?)
   //-----------------------------------
   {
      backCameras.clear()
      backCameraSizes.clear()
      frontCameras.clear()
      frontCameraSizes.clear()
      MAR.cameras(this, backCameras, backCameraSizes, null, true, false, WANT_HIGH_SPEED)
      MAR.cameras(this, frontCameras, frontCameraSizes, null, false, true, WANT_HIGH_SPEED)
      if ( (backCameras.isEmpty()) && (frontCameras.isEmpty()) )
      {
         message(this, "No cameras found")
         isCamerasList = true
         return
      }
      addSizes(backCameras, backCameraSizes)
      val noneAndBackCameras = mutableListOf<String>()
      noneAndBackCameras.add(MAR.None)
      if (backCameras.isNotEmpty())
         noneAndBackCameras.addAll(backCameras)
      if (frontCameras.isNotEmpty())
         addSizes(frontCameras, frontCameraSizes)

      camera0Spinner.adapter = ArrayAdapter(this, R.layout.spinner_item,
                                             noneAndBackCameras)
      camera1Spinner.adapter = ArrayAdapter(this, R.layout.spinner_item,
                                            noneAndBackCameras)
      val noneAndFrontCameras = mutableListOf<String>()
      noneAndFrontCameras.add(MAR.None)
      if (frontCameras.isNotEmpty())
         noneAndFrontCameras.addAll(frontCameras)
      camera2Spinner.adapter = ArrayAdapter(this, R.layout.spinner_item,
                                             noneAndFrontCameras)
   }

   inner class SelectCameraAdapter(val list: Spinner,
                                   val isFront: Boolean) : AdapterView.OnItemSelectedListener
   //=============================================================
   {
      var selectedCamera: String = MAR.None

      override fun onNothingSelected(vw: AdapterView<*>?) {}

      override fun onItemSelected(vw: AdapterView<*>?, p1: View?, p2: Int, p3: Long)
      //---------------------------------------------------------------------------
      {
         val spinner = vw as Spinner
         selectedCamera = if (spinner.selectedItem == null) MAR.None else spinner.selectedItem.toString()
         val sizeList = mutableListOf<String>()
         if (selectedCamera != MAR.None)
         {
            val sizes = cameraSizes.get(selectedCamera)
            if ( (sizes != null) && (sizes.isNotEmpty()) )
            {
               sizes.forEach { it -> sizeList.add("${it.width}x${it.height}") }
               if (sizeList.isEmpty())
                  sizeList.add(MAR.None)
               list.adapter = ArrayAdapter(this@MainActivity, R.layout.spinner_item, sizeList)
            }
         }
         if (sizeList.isEmpty())
         {
            sizeList.add(MAR.None)
            list.adapter = ArrayAdapter(this@MainActivity, R.layout.spinner_item, sizeList)
         }
      }
   }

   inner class SelectCameraSizeAdapter : AdapterView.OnItemSelectedListener
   //=============================================================
   {
      var selectedSize: Size? = null

      override fun onNothingSelected(vw: AdapterView<*>?) { selectedSize = null }

      override fun onItemSelected(vw: AdapterView<*>?, p1: View?, p2: Int, p3: Long)
      //---------------------------------------------------------------------------
      {
         val spinner = vw as Spinner
//         val sze =  None else spinner.selectedItem.toString()
         if (spinner.selectedItem == null)
            selectedSize = null
         else
         {
            val sze = spinner.selectedItem.toString()
            if (sze == MAR.None)
               selectedSize = null
            else
            {
               val a = sze.split("x")
               selectedSize = if (a.size < 2) null else Size(Integer.parseInt(a[0].trim()), Integer.parseInt(a[1].trim()))
            }
         }
      }
   }
}
