class Yarrharr < Formula
  desc "Command-line interface tool for managing media downloads using TMDB metadata"
  homepage "https://github.com/asleepynerd/yarrharr"
  version "2.4.2"
  url "https://github.com/asleepynerd/yarrharr/archive/refs/tags/v#{version}.tar.gz"
  sha256 "REPLACE_WITH_ACTUAL_SHA256"
  license "AGPL-3.0"

  depends_on "cmake" => :build
  depends_on "curl"
  depends_on "nlohmann-json"
  depends_on "ffmpeg" => :recommended

  def install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args
    system "cmake", "--build", "build"
    bin.install "build/yarrharr"
  end

  test do
    system "#{bin}/yarrharr", "--help"
  end
end 