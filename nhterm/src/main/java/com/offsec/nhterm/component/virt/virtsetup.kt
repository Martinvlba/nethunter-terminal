package com.offsec.nhterm.component.virt

import android.os.Build
import androidx.annotation.RequiresApi
import java.nio.file.Path

public class virtsetup(val installDir: Path) {
  @RequiresApi(Build.VERSION_CODES.O)
  private val rootPartition: Path = installDir.resolve(VirtDefaults.ROOTFS_FILENAME)

}
