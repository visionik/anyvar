# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Phase 1 scaffolding and core AnyVar C runtime implementation
- Unity-based test suite covering scalar, buffer, container, and error paths
- Cross-platform build, lint, and CI task/workflow setup for the C core library

### Changed
- Raised the project coverage policy and quality gate from 85% to 100%
- Updated project planning and specification artifacts to reflect the 100% coverage requirement
- Replaced the draft README specification with project usage, build workflow, and public API documentation
- Made LLVM tool discovery in the Taskfile portable across macOS and Linux, and updated macOS CI to expose Homebrew LLVM on PATH
- Updated CI to run pull-request checks for review-base branches, exposed POSIX pthread APIs to non-Windows builds for clang-tidy, and added a Windows-specific verification path to avoid current clang-tidy/MSVC SDK incompatibilities
- Fixed a cleanup leak in the runtime copy path for invalid map shapes and relaxed float-conversion warnings only for vendored Unity sources to keep Windows CI focused on project code
