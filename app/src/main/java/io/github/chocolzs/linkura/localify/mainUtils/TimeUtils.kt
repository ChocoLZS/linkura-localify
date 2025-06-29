package io.github.chocolzs.linkura.localify.mainUtils

import java.time.ZonedDateTime
import java.time.format.DateTimeFormatter
import java.time.ZoneId

object TimeUtils {
    fun convertIsoToLocalTime(isoTimeString: String): String {
        val zonedDateTime = ZonedDateTime.parse(isoTimeString, DateTimeFormatter.ISO_DATE_TIME)
        val currentZoneId = ZoneId.systemDefault()
        val localZonedDateTime = zonedDateTime.withZoneSameInstant(currentZoneId)
        val outputFormatter = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss")
        return localZonedDateTime.format(outputFormatter)
    }
}