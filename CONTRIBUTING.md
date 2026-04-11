# Contributing to Android-Mem-Kit

Thank you for considering contributing to Android-Mem-Kit! We welcome contributions from the security research community.

---

## Table of Contents

1. [Code of Conduct](#code-of-conduct)
2. [How Can I Contribute?](#how-can-i-contribute)
3. [Development Setup](#development-setup)
4. [Pull Request Guidelines](#pull-request-guidelines)
5. [Coding Standards](#coding-standards)
6. [Legal Notice](#legal-notice)

---

## Code of Conduct

### Our Pledge

We pledge to make participation in our project a harassment-free experience for everyone.

### Expected Behavior

- Be respectful and inclusive
- Accept constructive criticism
- Focus on what's best for the community
- Show empathy towards others

### Unacceptable Behavior

- Harassment or discrimination
- Trolling or insulting comments
- Publishing others' private information
- Promoting illegal activities

---

## How Can I Contribute?

### Reporting Bugs

1. Check if the bug is already reported
2. Use the [bug report template](.github/ISSUE_TEMPLATE/bug_report.md)
3. Include:
   - Clear description
   - Steps to reproduce
   - Expected vs actual behavior
   - Environment details (Android version, device, NDK)

### Suggesting Features

1. Check if the feature is already requested
2. Use the [feature request template](.github/ISSUE_TEMPLATE/feature_request.md)
3. Explain the security research use case

### Documentation

Improvements to documentation are always welcome:
- Fix typos or unclear explanations
- Add new examples
- Translate to other languages

### Code Contributions

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

---

## Development Setup

### Prerequisites

```bash
# Android NDK (r25b or newer)
export ANDROID_NDK_HOME=/path/to/android-ndk

# CMake 3.14+
cmake --version

# Git
git --version
```

### Clone and Build

```bash
# Fork and clone
git clone https://github.com/HanSoBored/Android-Mem-Kit.git
cd Android-Mem-Kit

# Setup dependencies
git submodule update --init --recursive

# Build
mkdir build && cd build
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-35
cmake --build .
```

### Build with Makefile (Simpler)

For a quicker build without CMake configuration:

```bash
# Simple build (uses Makefile)
make
make ANDROID_ABI=armeabi-v7a
make clean
```

### Testing

```bash
# Build test library
# Load on test device/emulator
# Check logcat output
adb logcat -s MemKit-Example
```

---

## Pull Request Guidelines

### Before Submitting

- [ ] Code follows coding standards
- [ ] Tests pass (if applicable)
- [ ] Documentation updated
- [ ] Commit messages are clear
- [ ] No sensitive information exposed

### PR Template

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Documentation update
- [ ] Code quality improvement

## Testing
How was this tested?

## Checklist
- [ ] I have read the CONTRIBUTING.md
- [ ] My code follows the project style
- [ ] I have commented my code where appropriate
- [ ] This is for security research purposes
```

---

## Coding Standards

### C Style

**Note:** This project uses **C23**. Contributors should be aware of C23 features such as `nullptr`, `[[nodiscard]]`, and `typeof_unqual`. Avoid using these features in ways that break compatibility with older compilers if possible.

```c
// Function naming: snake_case
uintptr_t memkit_get_lib_base(const char* lib_name);

// Variables: snake_case
static void* hook_stub = NULL;

// Constants: UPPER_SNAKE_CASE
#define MAX_RETRIES 30

// Types: PascalCase
typedef struct MemPatch MemPatch;

// Comments: Use // for single line, /* */ for blocks
```

### Error Handling

```c
// Always check return values
void* stub = memkit_hook(...);
if (!stub) {
    LOGE("Hook failed: %d", errno);
    return;
}

// Set errno on failure
if (!valid_input) {
    errno = EINVAL;
    return NULL;
}
```

### Logging

```c
#define LOG_TAG "MyModule"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
```

### Memory Safety

```c
// Always check allocations
void* ptr = malloc(size);
if (!ptr) {
    errno = ENOMEM;
    return NULL;
}

// Free in reverse order of allocation
free(inner_ptr);
free(outer_ptr);

// NULL after free (optional but recommended)
free(ptr);
ptr = NULL;
```

### Subproject Builds

When using MemKit as a CMake subproject (`add_subdirectory`), set `MEMKIT_BUILD_SHARED=OFF` to build a static library. This is the default for subproject builds:

```cmake
set(MEMKIT_BUILD_SHARED OFF)
add_subdirectory(path/to/Android-Mem-Kit)
```

---

## Documentation Standards

### Function Comments

```c
/**
 * Brief description
 *
 * Detailed description if needed
 *
 * @param param_name Description
 * @return Description of return value
 */
```

### Examples

All examples should:
- Be complete and compilable
- Include error handling
- Have clear comments
- Follow security research use cases

---

## Legal Notice

### Important

By contributing to this project, you confirm that:

1. **Your contributions are original** or you have permission to submit them
2. **You understand the purpose** of this library (security research, education)
3. **You will not use contributions** for illegal activities
4. **You agree to the license** (MIT)

### Prohibited Contributions

Do NOT submit:
- ❌ Code specifically designed for game cheating
- ❌ Circumvention tools for DRM/anti-tamper
- ❌ Malware or exploit code
- ❌ Code that violates third-party rights

### Allowed Contributions

DO submit:
- ✅ Security research tools
- ✅ Educational examples
- ✅ Bug fixes and improvements
- ✅ Documentation enhancements
- ✅ General instrumentation utilities

---

## Recognition

Contributors will be recognized in:
- README.md (Contributors section)
- Release notes (for significant contributions)
- Hall of Fame (optional)

---

## Questions?

- Open a [discussion](https://github.com/HanSoBored/Android-Mem-Kit/discussions)
- Check existing [issues](https://github.com/HanSoBored/Android-Mem-Kit/issues)

---

*Thank you for contributing to Android-Mem-Kit!*
