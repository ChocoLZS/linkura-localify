package io.github.chocolzs.linkura.localify.mainUtils

import kotlinx.serialization.ExperimentalSerializationApi
import kotlinx.serialization.json.Json

@OptIn(ExperimentalSerializationApi::class)
val json = Json {
    encodeDefaults = true
    ignoreUnknownKeys = true
    allowTrailingComma = true
    allowComments = true
}