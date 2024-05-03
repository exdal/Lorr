# Lorr

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A toy Vulkan-based renderer with bleeding edge features. Works on both Linux and Windows.

## Getting Started
To build, simply run `xmake` on project root directory.

llvm-clang with Ninja is preferred toolchain, for maximum performance and build speed.

You can configure your toolchain using `xmake f --toolchain=clang|cl|gcc --mode=debug|release` before running building.
Make sure to run `xmake install -o ./build` to get third party binaries.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
