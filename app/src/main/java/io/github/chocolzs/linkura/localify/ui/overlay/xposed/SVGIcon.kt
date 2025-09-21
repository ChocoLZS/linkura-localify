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

    object CopyTemplate {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = ""
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }

    object Info {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = "M11 17h2v-6h-2zm1-8q.425 0 .713-.288T13 8t-.288-.712T12 7t-.712.288T11 8t.288.713T12 9m0 13q-2.075 0-3.9-.788t-3.175-2.137T2.788 15.9T2 12t.788-3.9t2.137-3.175T8.1 2.788T12 2t3.9.788t3.175 2.137T21.213 8.1T22 12t-.788 3.9t-2.137 3.175t-3.175 2.138T12 22"
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }

    object PlayArrow {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = "M8 19V5l11 7z"
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }

    object Palette {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = "M12 22q-2.05 0-3.875-.788t-3.187-2.15t-2.15-3.187T2 12q0-2.075.813-3.9t2.2-3.175T8.25 2.788T12.2 2q2 0 3.775.688t3.113 1.9t2.125 2.875T22 11.05q0 2.875-1.75 4.413T16 17h-1.85q-.225 0-.312.125t-.088.275q0 .3.375.863t.375 1.287q0 1.25-.687 1.85T12 22m-5.5-9q.65 0 1.075-.425T8 11.5t-.425-1.075T6.5 10t-1.075.425T5 11.5t.425 1.075T6.5 13m3-4q.65 0 1.075-.425T11 7.5t-.425-1.075T9.5 6t-1.075.425T8 7.5t.425 1.075T9.5 9m5 0q.65 0 1.075-.425T16 7.5t-.425-1.075T14.5 6t-1.075.425T13 7.5t.425 1.075T14.5 9m3 4q.65 0 1.075-.425T19 11.5t-.425-1.075T17.5 10t-1.075.425T16 11.5t.425 1.075T17.5 13"
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }

    object Close {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = "M6.4 19L5 17.6l5.6-5.6L5 6.4L6.4 5l5.6 5.6L17.6 5L19 6.4L13.4 12l5.6 5.6l-1.4 1.4l-5.6-5.6z"
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }

    object KeyboardArrowRight {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = "M12.6 12L8 7.4L9.4 6l6 6l-6 6L8 16.6z"
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }

    object KeyboardArrowLeft {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = "m14 18l-6-6l6-6l1.4 1.4l-4.6 4.6l4.6 4.6z"
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }

    object PhotoCamera {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = "M12 17.5q1.875 0 3.188-1.312T16.5 13t-1.312-3.187T12 8.5T8.813 9.813T7.5 13t1.313 3.188T12 17.5m0-2q-1.05 0-1.775-.725T9.5 13t.725-1.775T12 10.5t1.775.725T14.5 13t-.725 1.775T12 15.5M4 21q-.825 0-1.412-.587T2 19V7q0-.825.588-1.412T4 5h3.15L9 3h6l1.85 2H20q.825 0 1.413.588T22 7v12q0 .825-.587 1.413T20 21z"
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }

    object Settings {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = "m9.25 22l-.4-3.2q-.325-.125-.612-.3t-.563-.375L4.7 19.375l-2.75-4.75l2.575-1.95Q4.5 12.5 4.5 12.338v-.675q0-.163.025-.338L1.95 9.375l2.75-4.75l2.975 1.25q.275-.2.575-.375t.6-.3l.4-3.2h5.5l.4 3.2q.325.125.613.3t.562.375l2.975-1.25l2.75 4.75l-2.575 1.95q.025.175.025.338v.674q0 .163-.05.338l2.575 1.95l-2.75 4.75l-2.95-1.25q-.275.2-.575.375t-.6.3l-.4 3.2zm2.8-6.5q1.45 0 2.475-1.025T15.55 12t-1.025-2.475T12.05 8.5q-1.475 0-2.488 1.025T8.55 12t1.013 2.475T12.05 15.5"
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }

    object DragIndicator {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = "M9 20q-.825 0-1.412-.587T7 18t.588-1.412T9 16t1.413.588T11 18t-.587 1.413T9 20m6 0q-.825 0-1.412-.587T13 18t.588-1.412T15 16t1.413.588T17 18t-.587 1.413T15 20m-6-6q-.825 0-1.412-.587T7 12t.588-1.412T9 10t1.413.588T11 12t-.587 1.413T9 14m6 0q-.825 0-1.412-.587T13 12t.588-1.412T15 10t1.413.588T17 12t-.587 1.413T15 14M9 8q-.825 0-1.412-.587T7 6t.588-1.412T9 4t1.413.588T11 6t-.587 1.413T9 8m6 0q-.825 0-1.412-.587T13 6t.588-1.412T15 4t1.413.588T17 6t-.587 1.413T15 8"
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }

    object Gamepad {
        fun createDrawable(context: Context, tintColor: Int = Color.WHITE, sizeDp: Float = 24f): Drawable {
            val pathData = "m12 10.5l-3-3V2h6v5.5zm4.5 4.5l-3-3l3-3H22v6zM2 15V9h5.5l3 3l-3 3zm7 7v-5.5l3-3l3 3V22z"
            return createSVGDrawable(context, pathData, tintColor, sizeDp)
        }
    }
}