class CustomGit < Formula
  desc "AI-powered git commit commands with semantic clustering"
  homepage "https://github.com/yp583/custom-git"
  url "https://github.com/yp583/homebrew-custom-git/archive/refs/tags/v2.0.1.tar.gz"
  sha256 "2ac72d6e04b10836a19d27634e6f3b2807a604d099d868f683f213148e7fb5da"
  license "MIT"

  head "https://github.com/yp583/custom-git.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "node"
  depends_on "openssl@3"

  def install
    # Allow CPM/FetchContent to download dependencies during build
    ENV["HOMEBREW_ALLOW_FETCHCONTENT"] = "1"

    # Build gcommit (C++ executable)
    # Note: Not using std_cmake_args to avoid FetchContent trap - CPM needs to download deps
    cd "commands/gcommit" do
      system "cmake", "-S", ".", "-B", "build",
             "-DCMAKE_BUILD_TYPE=Release",
             "-DCMAKE_VERBOSE_MAKEFILE=ON",
             "-DOPENSSL_ROOT_DIR=#{Formula["openssl@3"].opt_prefix}"
      system "cmake", "--build", "build", "--verbose"
      bin.install "build/git_gcommit.o"
    end

    # Build gcommit terminal-ui (Node.js CLI)
    cd "commands/gcommit/terminal-ui" do
      system "npm", "install"
      system "npm", "run", "build"
      bin.install "dist/cli.js" => "git-gcommit"
    end

    # Build mcommit (C++ executable)
    cd "commands/mcommit" do
      system "cmake", "-S", ".", "-B", "build",
             "-DCMAKE_BUILD_TYPE=Release",
             "-DCMAKE_VERBOSE_MAKEFILE=ON",
             "-DOPENSSL_ROOT_DIR=#{Formula["openssl@3"].opt_prefix}"
      system "cmake", "--build", "build", "--verbose"
      bin.install "build/git_mcommit.o"
      bin.install "git-mcommit"
    end

    # Install qcommit (bash script only)
    bin.install "commands/qcommit/git-qcommit"
  end

  def caveats
    <<~EOS
      Set your OpenAI API key using either method:

        Option 1: Environment variable
          export OPENAI_API_KEY="sk-..."

        Option 2: Git config (recommended)
          git config --global custom.openaiApiKey "sk-..."

      Available commands:
        git gcommit [-v]           Smart commit clustering with interactive UI
        git mcommit [-i]          AI-generated commit message (-i to edit in vim)
        git qcommit <n>           Quick commit with predefined message

      Setup qcommit:
        git config --global qcommit.m1 "merged"
        git config --global qcommit.m2 "wip"
        Then: git qcommit 1
    EOS
  end

  test do
    system bin/"git_gcommit.o", "--help"
    system bin/"git_mcommit.o", "--help"
    system bin/"git-qcommit", "--help"
  end
end
