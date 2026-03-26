# stb - Single File Libraries

This directory contains stb single-file public domain libraries.

## Files

- `stb_image.h` - Image loading library

## Download

stb libraries are single-file, public domain libraries by Sean Barrett.
Download from: https://github.com/nothings/stb

```bash
curl -L https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o extern/stb/stb_image.h
```

## Usage

Simply include the header files in your source code:

```cpp
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
```
