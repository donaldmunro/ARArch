package no.pack.drill.ararch.mar

import android.content.Context
import android.graphics.Point
import android.util.Size
import android.view.WindowManager
import kotlin.math.max
import kotlin.math.min

class SmartSize(width: Int, height: Int)
{
   var size = Size(width, height)
   var long = max(size.width, size.height)
   var short = min(size.width, size.height)
}

fun getMaximumOutputSize(sizes: Array<Size>): Size
{
   val comparator = compareBy { it: Size -> it.height * it.width }
   val sorted = sizes.sortedArrayWith(comparator) //sortedWith(compareBy { it.height * it.width }).reversed()[0]
   return sorted[0]
}

data class OutputSizeResult(val size: Size, val index: Int)

fun getPreviewOutputSize(context: Context, sizes: Array<Size>): OutputSizeResult
{
   // Find which is smaller: screen or 1080p
   val hdSize = SmartSize(1080, 720)
   val screenSize = getDisplaySmartSize(context)
   val hdScreen = screenSize.long >= hdSize.long || screenSize.short >= hdSize.short
   val maxSize = if (hdScreen) screenSize else hdSize

   // Get available sizes and sort them by area from largest to smallest
   val validSizes = sizes
         .sortedWith(compareBy { it.height * it.width })
         .map { SmartSize(it.width, it.height) }.reversed()

   // Then, get the largest output size that is smaller or equal than our max size
   val sz = validSizes.filter {
      it.long <= maxSize.long && it.short <= maxSize.short
   }[0].size
   val index = findSize(sizes, sz)
   return OutputSizeResult(sz, index)
}


fun getPreviewValidSizes(context: Context, surfaceSize: Size?, sizes: Array<Size>): Array<SmartSize>
//-----------------------------------------------------------------------------------------
{
   // Find which is smaller: screen or 1080p
   val hdSize = SmartSize(1080, 720)
   val displaySize: SmartSize
   if (surfaceSize == null)
      displaySize = getDisplaySmartSize(context)
   else
      displaySize = SmartSize(surfaceSize.width, surfaceSize.height)
   val hdScreen = displaySize.long >= hdSize.long || displaySize.short >= hdSize.short
   val maxSize = if (hdScreen) displaySize else hdSize

   // Get available sizes and sort them by area from largest to smallest
   var allValid = sizes.sortedWith(compareBy { it.height * it.width }).map { SmartSize(it.width, it.height) }.reversed()//.toTypedArray()
   return allValid.filter {
                            it.long <= maxSize.long && it.short <= maxSize.short }.toTypedArray()
}


fun getDisplaySmartSize(context: Context): SmartSize
{
   val windowManager = context.getSystemService(Context.WINDOW_SERVICE) as WindowManager
   val outPoint = Point()
   windowManager.defaultDisplay.getRealSize(outPoint)
   return SmartSize(outPoint.x, outPoint.y)
}

fun findSize(sizes: Array<Size>, size: Size): Int
{
   for (i in 0 until sizes.size)
   {
      val sz = sizes[i]
      if ( (sz.width == size.width) && (sz.height == size.height) )
         return i;
   }
   return -1
}
