# Local seeding adapter. For a public tap generate a literal formula with tools/seed_packages.py.
class EliteRingbuffer < Formula
  desc "C11 zero-copy SPSC and NCQ-SC64 POSIX shared-memory IPC"
  homepage ENV.fetch("ELITE_HOMEPAGE")
  url ENV.fetch("ELITE_SOURCE_URL")
  version "1.1.0"
  sha256 ENV.fetch("ELITE_SOURCE_SHA256")
  license "MIT"

  depends_on "cmake" => :build
  on_macos do
    depends_on macos: :sonoma
  end

  def install
    system "cmake", "-S", ".", "-B", "build-brew", *std_cmake_args,
           "-DELITE_BUILD_STATIC=ON", "-DELITE_BUILD_SHARED=ON",
           "-DELITE_BUILD_DEMO=ON", "-DELITE_BUILD_TESTS=OFF"
    system "cmake", "--build", "build-brew", "--parallel", "2"
    system "cmake", "--install", "build-brew"
  end

  def caveats
    <<~EOS
      macOS requires 14.4 or later and a natively admitted atomic ABI.
      This formula builds the C library and demo, not a Python interpreter extension.
      Demo percentiles are instrumented round-trip latency, not RTT/2 or a deadline.
    EOS
  end

  test do
    (testpath/"version.c").write <<~EOS
      #include <elite_api.h>
      int main(void) {
        return elite_version_number() == 0x00010100u ? 0 : 1;
      }
    EOS
    system ENV.cc, "-std=c11", "version.c", "-I#{include}", "-L#{lib}",
           "-lelite_ringbuffer", "-Wl,-rpath,#{lib}", "-o", "version"
    system "./version"
    system bin/"live_throughput_demo", "--mode", "spsc", "--windows", "1", "--messages", "1000", "--plain"
    system bin/"live_throughput_demo", "--mode", "ncq", "--windows", "1", "--messages", "1000", "--plain"
  end
end
