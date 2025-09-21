package io.github.chocolzs.linkura.localify.ui.overlay.xposed

import android.content.Context
import android.graphics.*
import android.graphics.drawable.Drawable
import android.util.Log
import androidx.core.graphics.PathParser
import kotlin.math.roundToInt

object SVGIcon {

    private fun dpToPx(context: Context, dp: Float): Int {
        val density = context.resources.displayMetrics.density
        return (dp * density).roundToInt()
    }

    private fun createSVGDrawable(context: Context, pathData: String, tintColor: Int, sizeDp: Float): Drawable {
        val sizePx = dpToPx(context, sizeDp)
        return object : Drawable() {
            private val paint = Paint().apply {
                isAntiAlias = true
                color = tintColor
                style = Paint.Style.FILL
            }

            private val path = try {
                PathParser.createPathFromPathData(pathData)
            } catch (e: Exception) {
                Log.e("SVGIcon", "Failed to parse path: $pathData", e)
                Path() // 空路径作为fallback
            }

            override fun draw(canvas: Canvas) {
                val bounds = bounds
                if (bounds.isEmpty) return

                canvas.save()

                // 计算缩放比例，使SVG适应bounds
                val pathBounds = RectF()
                path.computeBounds(pathBounds, true)

                if (pathBounds.width() > 0 && pathBounds.height() > 0) {
                    // 计算缩放比例，保持宽高比
                    val scaleX = bounds.width() / pathBounds.width()
                    val scaleY = bounds.height() / pathBounds.height()
                    val scale = minOf(scaleX, scaleY)

                    // 计算居中偏移
                    val scaledWidth = pathBounds.width() * scale
                    val scaledHeight = pathBounds.height() * scale
                    val offsetX = (bounds.width() - scaledWidth) / 2f
                    val offsetY = (bounds.height() - scaledHeight) / 2f

                    canvas.translate(bounds.left + offsetX, bounds.top + offsetY)
                    canvas.scale(scale, scale)
                    canvas.translate(-pathBounds.left, -pathBounds.top)
                } else {
                    // 如果路径边界无效，假设24x24的默认尺寸
                    val scale = minOf(bounds.width() / 24f, bounds.height() / 24f)
                    val offsetX = (bounds.width() - 24f * scale) / 2f
                    val offsetY = (bounds.height() - 24f * scale) / 2f

                    canvas.translate(bounds.left + offsetX, bounds.top + offsetY)
                    canvas.scale(scale, scale)
                }

                canvas.drawPath(path, paint)
                canvas.restore()
            }

            override fun setAlpha(alpha: Int) {
                paint.alpha = alpha
            }

            override fun setColorFilter(colorFilter: ColorFilter?) {
                paint.colorFilter = colorFilter
            }

            override fun getOpacity(): Int = PixelFormat.TRANSLUCENT

            override fun getIntrinsicWidth(): Int = sizePx
            override fun getIntrinsicHeight(): Int = sizePx
        }
    }

    object Camera {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = "M12,15.5A3.5,3.5 0 0,1 8.5,12A3.5,3.5 0 0,1 12,8.5A3.5,3.5 0 0,1 15.5,12A3.5,3.5 0 0,1 12,15.5M9,2L7.17,4H4C2.89,4 2,4.89 2,6V18A2,2 0 0,0 4,20H20A2,2 0 0,0 22,18V6C22,4.89 21.1,4 20,4H16.83L15,2H9Z"
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }

    object Settings {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = "M12,15.5A3.5,3.5 0 0,1 8.5,12A3.5,3.5 0 0,1 12,8.5A3.5,3.5 0 0,1 15.5,12A3.5,3.5 0 0,1 12,15.5M19.43,12.97C19.47,12.65 19.5,12.33 19.5,12C19.5,11.67 19.47,11.34 19.43,11L21.54,9.37C21.73,9.22 21.78,8.95 21.66,8.73L19.66,5.27C19.54,5.05 19.27,4.96 19.05,5.05L16.56,6.05C16.04,5.66 15.5,5.32 14.87,5.07L14.5,2.42C14.46,2.18 14.25,2 14,2H10C9.75,2 9.54,2.18 9.5,2.42L9.13,5.07C8.5,5.32 7.96,5.66 7.44,6.05L4.95,5.05C4.73,4.96 4.46,5.05 4.34,5.27L2.34,8.73C2.22,8.95 2.27,9.22 2.46,9.37L4.57,11C4.53,11.34 4.5,11.67 4.5,12C4.5,12.33 4.53,12.65 4.57,12.97L2.46,14.63C2.27,14.78 2.22,15.05 2.34,15.27L4.34,18.73C4.46,18.95 4.73,19.03 4.95,18.95L7.44,17.94C7.96,18.34 8.5,18.68 9.13,18.93L9.5,21.58C9.54,21.82 9.75,22 10,22H14C14.25,22 14.46,21.82 14.5,21.58L14.87,18.93C15.5,18.68 16.04,18.34 16.56,17.94L19.05,18.95C19.27,19.03 19.54,18.95 19.66,18.73L21.66,15.27C21.78,15.05 21.73,14.78 21.54,14.63L19.43,12.97Z"
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }

    object Menu {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = "M3,6H21V8H3V6M3,11H21V13H3V11M3,16H21V18H3V16Z"
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }

    object Close {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = "M19,6.41L17.59,5L12,10.59L6.41,5L5,6.41L10.59,12L5,17.59L6.41,19L12,13.41L17.59,19L19,17.59L13.41,12L19,6.41Z"
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }

    object Tool {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = "M22.7,19L13.6,9.9C14.5,7.6 14,4.9 12.1,3C10.1,1 7.1,0.6 4.7,1.7L9,6L6,9L1.6,4.7C0.4,7.1 0.9,10.1 2.9,12.1C4.8,14 7.5,14.5 9.8,13.6L18.9,22.7C19.3,23.1 19.9,23.1 20.3,22.7L22.6,20.4C23.1,20 23.1,19.3 22.7,19Z"
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }
}