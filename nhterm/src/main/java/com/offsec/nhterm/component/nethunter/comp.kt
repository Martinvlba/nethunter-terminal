package com.offsec.nhterm.component.nethunter

import android.content.Context
import android.system.Os
import android.util.Log
import com.offsec.nhterm.App
import com.offsec.nhterm.component.config.NeoTermPath
import com.offsec.nhterm.component.userscript.UserScript
import com.offsec.nhterm.utils.Executer
import com.offsec.nhterm.utils.extractAssetsDir
import com.topjohnwu.superuser.Shell
import java.io.File

// Martini was here :)
object NethunterComponent {
  // Simple pre-init stuff that gets launched on app launch
  fun init(context: Context) {
    // Pre-define variable/values
    val sysarch = System.getProperty("os.arch")
    val appuid = Os.getuid()
    val appgid = Os.getgid()
    val binDir = File(NeoTermPath.BIN_PATH + "_$sysarch")


    Log.d("NETHUNTER_INIT", "--- Start of NH init ---")

    // Cleanup old binaries
    Executer("/system/bin/mkdir -p /data/data/com.offsec.nhterm/files/usr/")
    Executer("/system/bin/rm -rf /data/data/com.offsec.nhterm/files/usr/bin")

    // Lets extract assets that fit for host architecture
    Log.d("NETHUNTER_INIT", "System arch is: $sysarch")
    if ( sysarch == "aarch64") {
      Log.d("NETHUNTER_INIT", "Extracting $sysarch binaries")
      Executer("/system/bin/rm -rf " + NeoTermPath.BIN_PATH + "_$sysarch")
      context.extractAssetsDir("bin_aarch64", NeoTermPath.BIN_PATH + "_$sysarch")
    } else if (sysarch == "armv7l") {
      Log.d("NETHUNTER_INIT", "Extracting $sysarch binaries")
      Executer("/system/bin/rm -rf " + NeoTermPath.BIN_PATH + "_$sysarch")
      context.extractAssetsDir("bin_armv7l", NeoTermPath.BIN_PATH + "_$sysarch")
    }

    // Chmod binaries
    binDir.listFiles()?.forEach {
      Os.chmod(NeoTermPath.BIN_PATH + "_$sysarch/${it.name}",448)
      Os.chown(NeoTermPath.BIN_PATH + "_$sysarch/${it.name}", appuid, appgid)
    }

    // Now symlink bin_${ARCH} to bin
    Executer("ln -svf /data/data/com.offsec.nhterm/files/usr/bin_$sysarch /data/data/com.offsec.nhterm/files/usr/bin")

    // Chown all the extracted stuff
    Log.d("NETHUNTER_INIT", "User UID: $appuid")
    Log.d("NETHUNTER_INIT", "User GID: $appgid")
    Executer("/system/bin/chown -R $appuid /data/data/com.offsec.nhterm/files/usr/bin*")

    // Determine correct system arch based binaries
    Log.d("NETHUNTER_INIT", "--- End of NH init ---")
  }
}
