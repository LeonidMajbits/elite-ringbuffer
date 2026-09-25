# Turn 16 primary source ledger — consulted 2026-09-24

These sources describe APIs, tooling and service configuration. They are not evidence that our CI has run, that a port is qualified, or that a particular performance bound holds.

R1. GitHub hosted-runner reference: https://docs.github.com/en/actions/reference/runners/github-hosted-runners . The retrieved public runner table lists macos-15 as ARM64 and macos-15-intel separately; actual uname is checked. Hosted runner resources are not the lab's M3 Max.

R2. GitHub secure-use reference: https://docs.github.com/en/actions/reference/security/secure-use . Pin action source, minimize permissions and avoid privileged execution of untrusted pull-request code. No secrets/self-hosted runners/write permissions are requested here.

R3. Selected checkout source: https://github.com/actions/checkout/commit/08c6903cd8c0fde910a37f88322edcfb5dd907a8 . v5.0.0, deliberately pinned rather than presented as latest.

R4. Selected setup-python source: https://github.com/actions/setup-python/commit/e797f83bcb11b83ae66e0230d6156d7c80228e7c . v6.0.0, deliberate full-source pin.

R5. Selected upload-artifact source: https://github.com/actions/upload-artifact/commit/b7c566a772e6b6bfb58ed0dc250532a479d7789f . v6.0.0, deliberate full-source pin.

R6. Author-maintained FreeBSD VM action definition: https://raw.githubusercontent.com/vmactions/freebsd-vm/77ed28d336d03fe19a3f4f7266c1d2c4714dd79d/action.yml . Selected v1.5.2 supports release/architecture/CPU/memory, synchronization, copyback and debug/cache controls. The action pin does not cryptographically freeze transitive OS images or pkg repositories. The VM lane is unexecuted here.

R7. CMake importing/exporting guide: https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html . Exported targets, generated package config and relocation. Actual Linux relocation tests are in our evidence, not inferred from this guide.

R8. Homebrew Formula Cookbook: https://docs.brew.sh/Formula-Cookbook . Formula install/test interfaces, checksums and CMake. Local Ruby syntax/generation is not a brew installation.

R9. Microsoft vcpkg overlay ports: https://learn.microsoft.com/en-us/vcpkg/concepts/overlay-ports . The supplied port is a local overlay in the complete source tree, not a registry submission.

R10. vcpkg CMake config fixup: https://learn.microsoft.com/en-us/vcpkg/maintainers/functions/vcpkg_cmake_config_fixup . Correct package-config placement and debug/release handling are delegated to the official host helper.

R11. mandoc manual: https://mandoc.bsd.lv/man/mandoc.1.html . CI uses -Tlint -Werror. No local mandoc/groff executable was available; roff checks here are structural, not rendered/manual-formatter qualification.

R12. Clang ThreadSanitizer and AddressSanitizer: https://clang.llvm.org/docs/ThreadSanitizer.html and https://clang.llvm.org/docs/AddressSanitizer.html . Instrumented runtime scopes remain separate; real detector activation is checked and TSan startup failure is not silently passed.

Internal authority: sealed Turn15 archive and its preserved report; release/LAB_RECEIPTS.md for earlier lab accounts; formal/SOURCE_BINDING.json for explicit review/source binding. No fresh Mac or FreeBSD execution follows from those historical records.

R13. FreeBSD release information: https://www.freebsd.org/releases/14.4R/announce/ and https://www.freebsd.org/releases/ . The configured 14.4 baseline is an explicit supported-series choice, not a claim that it is the newest point release (14.5 was also listed on the consultation date).
