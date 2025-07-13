package io.github.chocolzs.linkura.localify.models

import kotlinx.serialization.Serializable

@Serializable
data class AboutPageConfig(
    val plugin_repo: String = "https://github.com/ChocoLZS/linkura-localify",
    val main_contributors: List<MainContributors> = listOf(),
    val contrib_img: ContribImg = ContribImg(
        "https://contrib.rocks/image?repo=ChocoLZS/linkura-localify",
         "https://contrib.rocks/image?repo=chinosk6/GakumasTranslationData"
    )
)

@Serializable
data class MainContributors(
    val name: String,
    val links: List<Links>
)

@Serializable
data class ContribImg(
    val plugin: String,
    val translation: String,
    val translations: List<String> = listOf()
)

@Serializable
data class Links(
    val name: String,
    val link: String
)
