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

    var pf: Boolean = false
)
