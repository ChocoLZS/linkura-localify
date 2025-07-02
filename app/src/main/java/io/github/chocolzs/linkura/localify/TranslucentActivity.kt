package io.github.chocolzs.linkura.localify

import android.os.Bundle
import androidx.activity.ComponentActivity
import io.github.chocolzs.linkura.localify.models.LinkuraConfig
import io.github.chocolzs.linkura.localify.models.ProgramConfig


class TranslucentActivity : ComponentActivity(), IConfigurableActivity<TranslucentActivity> {
    override lateinit var config: LinkuraConfig
    override lateinit var programConfig: ProgramConfig

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        loadConfig()
        val requestData = intent.getStringExtra("l4Data")
        if (requestData != null) {
            if (requestData == "requestConfig") {
                onClickStartGame()
                finish()
            }
        }
    }
}
