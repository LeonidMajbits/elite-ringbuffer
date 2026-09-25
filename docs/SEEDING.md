# Open-source seeding kit — ELITEIPC 1.1.0

This is a new, separately hashed seeding derivative of the sealed Turn15 source
candidate. It is not an overwrite of its archive or an automatically pushed tag.
The inherited MIT license and NOTICE attribution remain unchanged. No public
repository name, release URL, badge result, bottle, registry listing or author
signature has been invented.

## Repository preparation

Review the diff against the known source. Exclude generated build directories and
private machine/grant logs. The kit deliberately retains the historical audit and
failed/incomplete evidence, so review that history for publication consent as well
as code correctness. The current manifests cover everything shipped; deleting or
editing evidence changes the source identity and needs a newly identified archive.

Use the existing repository and owner identity. Do not force-move a tag or replace
an already published v1.1.0 asset with different bytes. If v1.1.0 was already public,
name this seeding derivative separately or choose a reviewed subsequent release.
Enable GitHub's private vulnerability reporting before advertising that channel,
and choose branch protections after observing the actual CI job names.

The workflow is read-only CI/artifact delivery, not automatic CD to production.
Required jobs and experimental FreeBSD status are documented in CI_MATRIX.md.
No hosted job is marked passed merely because its YAML was generated.

## CMake and installed consumers

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release -DELITE_BUILD_TESTS=ON
cmake --build build/cmake --parallel 2
ctest --test-dir build/cmake --output-on-failure
cmake --install build/cmake --prefix "$PWD/build/install"
```

Choose `ELITE_BUILD_STATIC` and `ELITE_BUILD_SHARED`; both default ON and at least
one is required. `ELITE_BUILD_DEMO` defaults ON; CTest defaults OFF unless enabled.
The export supplies `elite::ringbuffer`, selecting shared when both exist, and
`elite::ringbuffer_static`/`elite::ringbuffer_shared` for available explicit forms.
Installed config files resolve their own prefix, with a separate version file.
The package includes pkg-config metadata, headers, libraries, man pages and license.
The existing Python exporter remains an optional GNU Make build, not a new hidden
CMake/Python runtime dependency. Install directory arguments should remain relative
to the selected prefix for relocatability.

A consumer uses:

```cmake
find_package(elite_ringbuffer 1.1 CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE elite::ringbuffer)
```

`tools/check_cmake_install.py` installs into a temporary prefix, moves it, builds
independent C/C++ consumers, executes the installed demo and verifies man files.
The FreeBSD configuration additionally requires
`-DELITE_EXPERIMENTAL_FREEBSD=ON`; it admits only the experimental polling profile.

## Homebrew

The top-level `elite-ringbuffer.rb` is an environment-parameterized local seeding
adapter. Public tap submissions should use a generated literal formula:

```sh
python3 tools/seed_packages.py --repository YOUR_OWNER/YOUR_REPOSITORY \
  --archive /absolute/path/reviewed-seeding-kit.zip \
  --url https://YOUR_PUBLIC_RELEASE_HOST/path/reviewed-seeding-kit.zip \
  --out /tmp/new-formula
```

Replace the explicitly named inputs with the actual intended publication. The tool
hashes the real archive; it never substitutes a tag's hypothetical checksum. It
checks bounded ZIP paths, expected package inputs and library version; run the
full release/capsule verifier separately before trusting the source. A URL and a
hash bind bytes, not author identity. `--local` produces a `file:` recipe for a
local tap test instead of making a public availability claim.

Copy the generated file into your chosen tap's Formula directory and run its
`brew install --build-from-source` and `brew test` steps. macOS needs 14.4+;
Linux requires its admitted native ABI. The test compiles an installed API consumer
and exercises both installed demo modes. No binary bottle is supplied.

## vcpkg

`vcpkg.json` is the project manifest. The local overlay includes a separate port
manifest and `portfile.cmake`, with host build helpers and no native framework
runtime dependencies. Pin your actual vcpkg tool/registry checkout independently.
From the complete verified source directory:

```sh
vcpkg install --classic --overlay-ports="$PWD/packaging/vcpkg" elite-ringbuffer:x64-linux
# On Apple Silicon use the admitted arm64-osx triplet.
```

The overlay intentionally uses this authenticated local source, not an invented
GitHub repository. It honors static/shared triplet linkage and applies CMake config
and pkg-config fixups. Copying the port directory alone without the repository is
rejected. The root manifest has no dependency on itself; consumers explicitly add
the port. Public registry acceptance and Windows support are not claimed.

## UNIX manual pages and demo

`make install` includes sections 3 and 7. `make install-demo` additionally installs
the console demo. CMake installs all selected native artifacts. Inspect with
`man 3 elite_ringbuffer` and `man 7 eliteipc`, or lint source with
`mandoc -Tlint -Werror man/elite_ringbuffer.3 man/eliteipc.7`.

See LIVE_DEMO.md for exact metric and replay semantics. It uses real POSIX payload
transport and an exec'd responder; its display is not prerecorded and does not
borrow the lab's historic rates. Raw RTT samples are optionally exported. No
CI-host speed is used to infer a universal Apple or Linux performance guarantee.

## Byte identity and changes

Run `python3 tools/verify_release.py --strict` before building a fresh archive, or
without `--strict` in a working Git/build directory. Verification is read-only.
After intentional source changes, the maintainer runs the separate
`tools/regenerate_manifests.py`, reviews the delta, and seals a new archive. That
operation updates byte inventories, not historical test outcomes or canonical
report contents. SHA-256 sidecars are integrity receipts, not digital signatures.
