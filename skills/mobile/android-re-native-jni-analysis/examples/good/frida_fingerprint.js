/* Correct: fingerprint-first native analysis hook for Frida.
 * Target flow (frida toolchain not installed on this host):
 *   frida -U -l examples/good/frida_fingerprint.js com.example.app
 * The script fingerprints the .so (exports + JNI registration) before any
 * interception, so hooks are placed on REAL addresses, not guessed ones.
 */
"use strict";

const MODULE = "libnative-lib.so";

function fingerprint() {
    const mod = Process.findModuleByName(MODULE);
    if (mod === null) {
        console.log("module not loaded yet");
        return;
    }
    console.log("base: " + mod.base + " size: " + mod.size);
    mod.enumerateExports().forEach((e) => {
        console.log("export " + e.name + " -> " + e.address);
    });
    // JNI_OnLoad registrations can be found by scanning the export table
    // and by hooking RegisterNatives if used (seen in many apps).
}

fingerprint();
