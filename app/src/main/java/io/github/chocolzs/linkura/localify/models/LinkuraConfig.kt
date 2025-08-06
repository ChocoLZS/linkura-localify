package io.github.chocolzs.linkura.localify.models

import kotlinx.serialization.Serializable

@Serializable
data class LinkuraConfig (
    var dbgMode: Boolean = false,
    var enabled: Boolean = true,
    var renderHighResolution: Boolean = true,
    var fesArchiveUnlockTicket: Boolean = false,
    var lazyInit: Boolean = true,
    var replaceFont: Boolean = true,
    var textTest: Boolean = false,
    var dumpText: Boolean = false,
    var forceExportResource: Boolean = false,
    var enableFreeCamera: Boolean = false,
    var targetFrameRate: Int = 0,
    var removeRenderImageCover: Boolean = false,
    var avoidCharacterExit: Boolean = false,

    var storyHideBackground: Boolean = false,
    var storyHideTransition: Boolean = false,
    var storyHideNonCharacter3d: Boolean = false,
    var storyHideDof: Boolean = false,
    var storyNovelVocalTextDurationRate: Float = 1.0f,
    var storyNovelNonVocalTextDurationRate: Float = 1.0f,

    var firstPersonCameraHideHead: Boolean = true,
    var firstPersonCameraHideHair: Boolean = true,

    var enableMotionCaptureReplay: Boolean = true,
    var enableInGameReplayDisplay: Boolean = true,
    var motionCaptureResourceUrl: String = "https://assets.chocoie.com",
    var withliveOrientation: Int = 2,
    var lockRenderTextureResolution: Boolean = false,
    var renderTextureLongSide: Int = 3840,
    var renderTextureShortSide: Int = 2160,
    var hideCharacterBody: Boolean = false,
    var renderTextureAntiAliasing: Int = 0,

    var pf: Boolean = false,

)
