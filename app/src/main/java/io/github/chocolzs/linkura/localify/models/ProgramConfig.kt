package io.github.chocolzs.linkura.localify.models

import io.github.chocolzs.linkura.localify.mainUtils.json
import kotlinx.serialization.KSerializer
import kotlinx.serialization.Serializable
import kotlinx.serialization.descriptors.SerialDescriptor
import kotlinx.serialization.encoding.Decoder
import kotlinx.serialization.encoding.Encoder
import kotlinx.serialization.encoding.encodeStructure
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.encodeToJsonElement
import kotlinx.serialization.json.jsonObject

@Serializable
data class ProgramConfig(
    var checkBuiltInAssets: Boolean = true,
    var transRemoteZipUrl: String = "",
    var useRemoteAssets: Boolean = false,
    var useAPIAssets: Boolean = true,
    var useAPIAssetsURL: String = "",
    var delRemoteAfterUpdate: Boolean = true,
    var cleanLocalAssets: Boolean = false,

    // var localAPIAssetsVersion: String = "",
    var p: Boolean = false
)

class ProgramConfigSerializer(
    private val excludes: List<String> = emptyList(),
) : KSerializer<ProgramConfig> {
    override val descriptor: SerialDescriptor = ProgramConfig.serializer().descriptor
    override fun serialize(encoder: Encoder, value: ProgramConfig) {
        val jsonObject = json.encodeToJsonElement(value).jsonObject
        encoder.encodeStructure(descriptor) {
            jsonObject.keys.forEachIndexed { index, k ->
                if (k in excludes) return@forEachIndexed
                encodeSerializableElement(descriptor, index, JsonElement.serializer(), jsonObject[k]!!)
            }
        }
    }

    override fun deserialize(decoder: Decoder): ProgramConfig {
        return ProgramConfig.serializer().deserialize(decoder)
    }
}
