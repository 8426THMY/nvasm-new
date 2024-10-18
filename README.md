
# nvasm-new
In the days before HLSL was mainstream, many old DirectX 8 applications used a program called "nvasm.exe" to compile their assembly shaders. This program was particularly useful, as it allowed for the use of C-style preprocessor macros. ~~Unfortunately, this program is extremely hard to find these days, as NVIDIA have since dropped it from their website, and no archives currently exist (to my knowledge).~~ Thanks to [Kllrt](https://github.com/Kllrt), a version of this program has been found on the Internet Archive! It can be downloaded from the link in the [pinned issue](https://github.com/8426THMY/nvasm-new/issues/1). As well as containing a copy of the binary, this link also has some documentation which tells us the full extent of the program's capabilities.

This application doesn't reproduce the entirety of nvasm's functionality, but it does allow the compilation of shaders that use C preprocessor directives (specifically, "include" and "define"). By default, the compiled shader is output in a binary format. However, if the "-h" command-line argument is specified, it will instead be output as a header that can be simply included in your project.

# Dependencies
While no special prerequisites are required to _compile_ this application, the DirectX 8 shader assemblers "vsa.exe" and "psa.exe" are required to actually use it successfully. Luckily, unlike nvasm, these applications are still available from sufficiently old versions of the DirectX 8 SDK.

# Usage
The latest Windows binary for nvasm-new can be downloaded in the releases section. The application may be used as follows:

`nvasm.exe -[OPTIONS] "shader_path" "output_path"`

While the shader path is required, the output path is not: if none is specified, it will use whatever vsa or psa output by default.

Currently, the only valid option is:

| Option | Description |
| --- | --- |
| h | Output the shader as a header file. |
