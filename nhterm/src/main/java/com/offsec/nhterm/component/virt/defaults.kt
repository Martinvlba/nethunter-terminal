package com.offsec.nhterm.component.virt

object VirtDefaults {
  const val INSTALL_DIRNAME = "linux"
  const val ROOTFS_FILENAME = "root_part"
  const val BACKUP_FILENAME = "root_part_backup"
  const val CONFIG_FILENAME = "vm_config.json"
  const val BUILD_ID_FILENAME = "build_id"
  const val MARKER_FILENAME: String = "completed"

  const val RESIZE_STEP_BYTES: Long = 4 shl 20 // 4 MiB
}
