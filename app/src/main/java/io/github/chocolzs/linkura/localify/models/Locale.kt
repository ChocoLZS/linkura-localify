package io.github.chocolzs.linkura.localify.models

import kotlinx.serialization.Serializable

@Serializable
data class LocaleItem(
    val name: String,
    val code: String
)
