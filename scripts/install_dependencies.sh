#!/bin/bash
# TrinityCore PlayerBot - Dependency Installation Script
# Runs automatically on SessionStart in Claude Code Web

set -e

echo "=== TrinityCore PlayerBot Dependency Setup ==="
echo ""

# Only run in remote (web) environments
if [ "$CLAUDE_CODE_REMOTE" != "true" ]; then
  echo "Skipping dependency installation (local environment)"
  exit 0
fi

echo "Running in Claude Code Web environment - checking dependencies..."

# Check if Boost is already set up
if [ -d "/home/user/boost_1_83_0" ] && [ -f "/home/user/boost_1_83_0/stage/lib/libboost_filesystem.a" ]; then
  echo "✅ Boost 1.83.0 already installed"
  export BOOST_ROOT=/home/user/boost_1_83_0
  echo "export BOOST_ROOT=/home/user/boost_1_83_0" >> "${CLAUDE_ENV_FILE}"
  exit 0
fi

echo "📦 Setting up Boost 1.83.0 from GitHub..."

cd /home/user

# Clone Boost if not already present
if [ ! -d "boost_1_83_0" ]; then
  echo "Cloning Boost 1.83.0..."
  git clone --depth 1 --branch boost-1.83.0 https://github.com/boostorg/boost.git boost_1_83_0
fi

cd boost_1_83_0

# Initialize required submodules
echo "Initializing Boost submodules..."
git submodule update --init --depth 1 \
  libs/filesystem \
  libs/program_options \
  libs/regex \
  libs/locale \
  libs/thread \
  libs/system \
  libs/config \
  libs/core \
  libs/detail \
  libs/type_traits \
  libs/static_assert \
  libs/preprocessor \
  libs/mpl \
  libs/assert \
  libs/throw_exception \
  tools/build \
  tools/boost_install

# Bootstrap if needed
if [ ! -f "b2" ]; then
  echo "Bootstrapping Boost build system..."
  ./bootstrap.sh --with-libraries=filesystem,program_options,regex,locale,thread
fi

# Generate headers
echo "Generating Boost headers..."
./b2 headers

# Build required libraries
echo "Building Boost libraries (this may take 5-10 minutes)..."
./b2 --with-filesystem --with-program_options --with-regex --with-locale --with-thread \
  -j4 variant=release link=static runtime-link=shared

echo ""
echo "✅ Boost 1.83.0 setup complete!"
echo ""

# Export BOOST_ROOT for the session
export BOOST_ROOT=/home/user/boost_1_83_0
echo "export BOOST_ROOT=/home/user/boost_1_83_0" >> "${CLAUDE_ENV_FILE}"

echo "BOOST_ROOT=/home/user/boost_1_83_0"
echo ""
echo "Ready to build TrinityCore PlayerBot module!"

exit 0
