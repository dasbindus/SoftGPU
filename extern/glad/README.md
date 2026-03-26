# glad - OpenGL Loader

This is a placeholder for the glad OpenGL loader.

## Installation

To generate glad files, visit https://glad.dav1d.de/ with the following settings:
- Language: C/C++
- Specification: OpenGL
- API: gl=4.6, gles1=off, gles2=off, glsc2=off
- Profile: Core
- Extensions: leave empty

Or use the glad CLI:
```bash
pip install glad
python -m glad --out-path=extern/glad --generator=c --spec=gl --version=4.6 --profile=core
```

## Required Files

After generation, copy the contents to:
- `include/glad/glad.h`
- `include/glad/khrplatform.h`
- `src/glad.c`
